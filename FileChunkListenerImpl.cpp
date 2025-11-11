#include "FileChunkListenerImpl.h"
#include "FileUtils.h"
#include "Checksum.h"

#include <ace/Log_Msg.h>

namespace DirShare {

FileChunkListenerImpl::FileChunkListenerImpl(const std::string& shared_dir,
                                               FileChangeTracker& change_tracker)
  : shared_dir_(shared_dir)
  , change_tracker_(change_tracker)
{
}

FileChunkListenerImpl::~FileChunkListenerImpl()
{
}

void FileChunkListenerImpl::on_requested_deadline_missed(
  DDS::DataReader_ptr,
  const DDS::RequestedDeadlineMissedStatus&)
{
}

void FileChunkListenerImpl::on_requested_incompatible_qos(
  DDS::DataReader_ptr,
  const DDS::RequestedIncompatibleQosStatus&)
{
}

void FileChunkListenerImpl::on_sample_rejected(
  DDS::DataReader_ptr,
  const DDS::SampleRejectedStatus&)
{
}

void FileChunkListenerImpl::on_liveliness_changed(
  DDS::DataReader_ptr,
  const DDS::LivelinessChangedStatus&)
{
}

void FileChunkListenerImpl::on_subscription_matched(
  DDS::DataReader_ptr,
  const DDS::SubscriptionMatchedStatus&)
{
}

void FileChunkListenerImpl::on_sample_lost(
  DDS::DataReader_ptr,
  const DDS::SampleLostStatus&)
{
}

void FileChunkListenerImpl::on_data_available(DDS::DataReader_ptr reader)
{
  FileChunkDataReader_var chunk_reader =
    FileChunkDataReader::_narrow(reader);

  if (!chunk_reader) {
    ACE_ERROR((LM_ERROR,
               ACE_TEXT("ERROR: %N:%l: FileChunkListenerImpl::on_data_available() - ")
               ACE_TEXT("failed to narrow DataReader!\n")));
    return;
  }

  FileChunk chunk;
  DDS::SampleInfo info;

  DDS::ReturnCode_t status = chunk_reader->take_next_sample(chunk, info);

  if (status == DDS::RETCODE_OK) {
    if (info.valid_data) {
      ACE_DEBUG((LM_DEBUG,
                 ACE_TEXT("(%P|%t) Received FileChunk: %C chunk %u/%u (%u bytes)\n"),
                 chunk.filename.in(),
                 chunk.chunk_id + 1,
                 chunk.total_chunks,
                 chunk.data.length()));

      process_chunk(chunk);
    }
  } else if (status != DDS::RETCODE_NO_DATA) {
    ACE_ERROR((LM_ERROR,
               ACE_TEXT("ERROR: %N:%l: FileChunkListenerImpl::on_data_available() - ")
               ACE_TEXT("take_next_sample failed: %d\n"),
               status));
  }
}

void FileChunkListenerImpl::process_chunk(const FileChunk& chunk)
{
  std::string filename = chunk.filename.in();

  // Verify chunk checksum
  if (chunk.data.length() > 0) {
    uint32_t computed_checksum = compute_checksum(
      reinterpret_cast<const uint8_t*>(chunk.data.get_buffer()),
      chunk.data.length());

    if (computed_checksum != chunk.chunk_checksum) {
      ACE_ERROR((LM_ERROR,
                 ACE_TEXT("ERROR: %N:%l: Chunk checksum mismatch for %C chunk %u\n")
                 ACE_TEXT("  Expected: 0x%08X, Computed: 0x%08X\n"),
                 filename.c_str(),
                 chunk.chunk_id,
                 chunk.chunk_checksum,
                 computed_checksum));
      return;
    }
  }

  // Get or create reassembly buffer for this file
  ChunkedFile& chunked_file = reassembly_buffer_[filename];

  // Initialize on first chunk
  if (chunked_file.total_chunks == 0) {
    chunked_file.total_chunks = chunk.total_chunks;
    chunked_file.file_size = chunk.file_size;
    chunked_file.file_checksum = chunk.file_checksum;
    chunked_file.timestamp_sec = chunk.timestamp_sec;
    chunked_file.timestamp_nsec = chunk.timestamp_nsec;
    chunked_file.data.resize(chunk.file_size);

    ACE_DEBUG((LM_INFO,
               ACE_TEXT("(%P|%t) Starting reassembly of file: %C (%Q bytes, %u chunks)\n"),
               filename.c_str(),
               chunk.file_size,
               chunk.total_chunks));
  }

  // Validate consistency
  if (chunk.total_chunks != chunked_file.total_chunks ||
      chunk.file_size != chunked_file.file_size ||
      chunk.file_checksum != chunked_file.file_checksum) {
    ACE_ERROR((LM_ERROR,
               ACE_TEXT("ERROR: %N:%l: Inconsistent chunk metadata for %C chunk %u\n"),
               filename.c_str(),
               chunk.chunk_id));
    return;
  }

  // Copy chunk data into reassembly buffer
  const uint32_t chunk_size = 1024 * 1024; // 1MB
  uint64_t offset = static_cast<uint64_t>(chunk.chunk_id) * chunk_size;

  if (offset + chunk.data.length() > chunked_file.file_size) {
    ACE_ERROR((LM_ERROR,
               ACE_TEXT("ERROR: %N:%l: Chunk data exceeds file size for %C chunk %u\n"),
               filename.c_str(),
               chunk.chunk_id));
    return;
  }

  std::memcpy(&chunked_file.data[offset],
              chunk.data.get_buffer(),
              chunk.data.length());

  chunked_file.received_chunks[chunk.chunk_id] = true;

  ACE_DEBUG((LM_DEBUG,
             ACE_TEXT("(%P|%t) Reassembly progress for %C: %u/%u chunks received\n"),
             filename.c_str(),
             static_cast<unsigned int>(chunked_file.received_chunks.size()),
             chunked_file.total_chunks));

  // Check if file is complete
  if (chunked_file.is_complete()) {
    ACE_DEBUG((LM_INFO,
               ACE_TEXT("(%P|%t) All chunks received for %C, finalizing...\n"),
               filename.c_str()));

    finalize_file(filename, chunked_file);

    // Remove from reassembly buffer
    reassembly_buffer_.erase(filename);
  }
}

void FileChunkListenerImpl::finalize_file(
  const std::string& filename,
  ChunkedFile& chunked_file)
{
  std::string full_path = shared_dir_ + "/" + filename;

  // Check if file exists and compare timestamps for MODIFY case
  if (file_exists(full_path)) {
    unsigned long long local_timestamp_sec;
    unsigned long local_timestamp_nsec;
    if (get_file_mtime(full_path, local_timestamp_sec, local_timestamp_nsec)) {
      // Compare remote timestamp with local timestamp
      bool remote_is_newer = false;
      if (chunked_file.timestamp_sec > local_timestamp_sec) {
        remote_is_newer = true;
      } else if (chunked_file.timestamp_sec == local_timestamp_sec &&
                 chunked_file.timestamp_nsec > local_timestamp_nsec) {
        remote_is_newer = true;
      }

      if (!remote_is_newer) {
        ACE_DEBUG((LM_INFO,
                   ACE_TEXT("(%P|%t) Local file is newer or same, ignoring FileChunk reassembly for: %C\n"),
                   filename.c_str()));
        // Resume notifications even when rejecting update (SC-011: prevent permanent suppression)
        change_tracker_.resume_notifications(filename);
        return;
      }

      ACE_DEBUG((LM_INFO,
                 ACE_TEXT("(%P|%t) Remote file is newer, updating local file with reassembled chunks: %C\n"),
                 filename.c_str()));
    }
  }

  // Verify file checksum
  uint32_t computed_checksum = compute_checksum(
    &chunked_file.data[0],
    chunked_file.file_size);

  if (computed_checksum != chunked_file.file_checksum) {
    ACE_ERROR((LM_ERROR,
               ACE_TEXT("ERROR: %N:%l: File checksum mismatch after reassembly for %C\n")
               ACE_TEXT("  Expected: 0x%08X, Computed: 0x%08X\n"),
               filename.c_str(),
               chunked_file.file_checksum,
               computed_checksum));
    // Resume notifications on error (SC-011: prevent permanent suppression)
    change_tracker_.resume_notifications(filename);
    return;
  }

  // Write file
  if (!write_file(full_path,
                  &chunked_file.data[0],
                  chunked_file.file_size)) {
    ACE_ERROR((LM_ERROR,
               ACE_TEXT("ERROR: %N:%l: Failed to write reassembled file: %C\n"),
               full_path.c_str()));
    // Resume notifications on error (SC-011: prevent permanent suppression)
    change_tracker_.resume_notifications(filename);
    return;
  }

  // Preserve timestamp
  if (!set_file_mtime(full_path, chunked_file.timestamp_sec, chunked_file.timestamp_nsec)) {
    ACE_ERROR((LM_WARNING,
               ACE_TEXT("WARNING: %N:%l: Failed to set timestamp for file: %C\n"),
               full_path.c_str()));
    // Don't fail the operation - file was written successfully
  }

  ACE_DEBUG((LM_INFO,
             ACE_TEXT("(%P|%t) Successfully wrote reassembled file: %C (%Q bytes, checksum: 0x%08X)\n"),
             filename.c_str(),
             chunked_file.file_size,
             chunked_file.file_checksum));

  // Resume notifications for this file (SC-011: prevent notification loop)
  change_tracker_.resume_notifications(filename);
  ACE_DEBUG((LM_DEBUG,
             ACE_TEXT("(%P|%t) Resumed notifications for file: %C\n"),
             filename.c_str()));
}

} // namespace DirShare
