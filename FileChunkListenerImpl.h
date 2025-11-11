#ifndef DIRSHARE_FILE_CHUNK_LISTENER_IMPL_H
#define DIRSHARE_FILE_CHUNK_LISTENER_IMPL_H

#include "DirShareTypeSupportImpl.h"
#include "FileChangeTracker.h"

#include <dds/DCPS/LocalObject.h>
#include <dds/DdsDcpsSubscriptionC.h>

#include <string>
#include <map>
#include <vector>

namespace DirShare {

// Structure to track reassembly of chunked files
struct ChunkedFile {
  std::vector<uint8_t> data;
  std::map<uint32_t, bool> received_chunks;
  uint32_t total_chunks;
  uint64_t file_size;
  uint32_t file_checksum;
  uint64_t timestamp_sec;
  uint32_t timestamp_nsec;

  ChunkedFile()
    : total_chunks(0)
    , file_size(0)
    , file_checksum(0)
    , timestamp_sec(0)
    , timestamp_nsec(0)
  {
  }

  bool is_complete() const {
    if (received_chunks.size() != total_chunks) {
      return false;
    }
    for (uint32_t i = 0; i < total_chunks; ++i) {
      std::map<uint32_t, bool>::const_iterator it = received_chunks.find(i);
      if (it == received_chunks.end() || !it->second) {
        return false;
      }
    }
    return true;
  }
};

class FileChunkListenerImpl
  : public virtual OpenDDS::DCPS::LocalObject<DDS::DataReaderListener>
{
public:
  explicit FileChunkListenerImpl(const std::string& shared_dir,
                                  FileChangeTracker& change_tracker);

  virtual ~FileChunkListenerImpl();

  virtual void on_requested_deadline_missed(
    DDS::DataReader_ptr reader,
    const DDS::RequestedDeadlineMissedStatus& status);

  virtual void on_requested_incompatible_qos(
    DDS::DataReader_ptr reader,
    const DDS::RequestedIncompatibleQosStatus& status);

  virtual void on_sample_rejected(
    DDS::DataReader_ptr reader,
    const DDS::SampleRejectedStatus& status);

  virtual void on_liveliness_changed(
    DDS::DataReader_ptr reader,
    const DDS::LivelinessChangedStatus& status);

  virtual void on_data_available(DDS::DataReader_ptr reader);

  virtual void on_subscription_matched(
    DDS::DataReader_ptr reader,
    const DDS::SubscriptionMatchedStatus& status);

  virtual void on_sample_lost(
    DDS::DataReader_ptr reader,
    const DDS::SampleLostStatus& status);

private:
  std::string shared_dir_;
  FileChangeTracker& change_tracker_;  // Reference to shared tracker for loop prevention
  std::map<std::string, ChunkedFile> reassembly_buffer_;

  // Process received chunk
  void process_chunk(const FileChunk& chunk);

  // Finalize reassembled file
  void finalize_file(const std::string& filename, ChunkedFile& chunked_file);
};

} // namespace DirShare

#endif // DIRSHARE_FILE_CHUNK_LISTENER_IMPL_H
