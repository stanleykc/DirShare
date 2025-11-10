#include "SnapshotListenerImpl.h"
#include "FileUtils.h"
#include "Checksum.h"

#include <ace/Log_Msg.h>

#include <set>

namespace DirShare {

SnapshotListenerImpl::SnapshotListenerImpl(
  const std::string& shared_dir,
  DDS::DataWriter_ptr content_writer,
  DDS::DataWriter_ptr chunk_writer)
  : shared_dir_(shared_dir)
  , content_writer_(DDS::DataWriter::_duplicate(content_writer))
  , chunk_writer_(DDS::DataWriter::_duplicate(chunk_writer))
{
}

SnapshotListenerImpl::~SnapshotListenerImpl()
{
}

void SnapshotListenerImpl::on_requested_deadline_missed(
  DDS::DataReader_ptr,
  const DDS::RequestedDeadlineMissedStatus&)
{
}

void SnapshotListenerImpl::on_requested_incompatible_qos(
  DDS::DataReader_ptr,
  const DDS::RequestedIncompatibleQosStatus&)
{
}

void SnapshotListenerImpl::on_sample_rejected(
  DDS::DataReader_ptr,
  const DDS::SampleRejectedStatus&)
{
}

void SnapshotListenerImpl::on_liveliness_changed(
  DDS::DataReader_ptr,
  const DDS::LivelinessChangedStatus&)
{
}

void SnapshotListenerImpl::on_subscription_matched(
  DDS::DataReader_ptr,
  const DDS::SubscriptionMatchedStatus&)
{
}

void SnapshotListenerImpl::on_sample_lost(
  DDS::DataReader_ptr,
  const DDS::SampleLostStatus&)
{
}

void SnapshotListenerImpl::on_data_available(DDS::DataReader_ptr reader)
{
  DirectorySnapshotDataReader_var snapshot_reader =
    DirectorySnapshotDataReader::_narrow(reader);

  if (!snapshot_reader) {
    ACE_ERROR((LM_ERROR,
               ACE_TEXT("ERROR: %N:%l: SnapshotListenerImpl::on_data_available() - ")
               ACE_TEXT("failed to narrow DataReader!\n")));
    return;
  }

  DirectorySnapshot snapshot;
  DDS::SampleInfo info;

  DDS::ReturnCode_t status = snapshot_reader->take_next_sample(snapshot, info);

  if (status == DDS::RETCODE_OK) {
    if (info.valid_data) {
      ACE_DEBUG((LM_INFO,
                 ACE_TEXT("(%P|%t) Received DirectorySnapshot from participant %C\n")
                 ACE_TEXT("  File count: %u\n"),
                 snapshot.participant_id.in(),
                 snapshot.file_count));

      process_snapshot(snapshot);
    }
  } else if (status != DDS::RETCODE_NO_DATA) {
    ACE_ERROR((LM_ERROR,
               ACE_TEXT("ERROR: %N:%l: SnapshotListenerImpl::on_data_available() - ")
               ACE_TEXT("take_next_sample failed: %d\n"),
               status));
  }
}

void SnapshotListenerImpl::process_snapshot(const DirectorySnapshot& snapshot)
{
  // Build set of local files
  std::set<std::string> local_files;
  std::vector<std::string> files;
  list_directory_files(shared_dir_, files);

  for (size_t i = 0; i < files.size(); ++i) {
    local_files.insert(files[i]);
  }

  // Check each file in the snapshot
  for (CORBA::ULong i = 0; i < snapshot.files.length(); ++i) {
    const FileMetadata& metadata = snapshot.files[i];
    std::string filename = metadata.filename.in();

    // Check if we have this file locally
    if (local_files.find(filename) == local_files.end()) {
      // File missing locally - request it
      ACE_DEBUG((LM_INFO,
                 ACE_TEXT("(%P|%t) File missing locally: %C (size: %Q bytes)\n"),
                 filename.c_str(),
                 metadata.size));

      request_file(metadata);
    } else {
      // File exists - could check if our version is older
      // For now, skip files that exist locally
      ACE_DEBUG((LM_DEBUG,
                 ACE_TEXT("(%P|%t) File already exists locally: %C\n"),
                 filename.c_str()));
    }
  }
}

void SnapshotListenerImpl::request_file(const FileMetadata& metadata)
{
  // In initial implementation, we'll trigger a file transfer request
  // This is a placeholder - the actual mechanism would involve:
  // 1. Sending a FileEvent request (future enhancement)
  // 2. Or relying on the remote side to publish FileContent/FileChunks
  //    for files in their snapshot (which is the current approach)

  ACE_DEBUG((LM_INFO,
             ACE_TEXT("(%P|%t) Requesting file: %C from remote participant\n"),
             metadata.filename.in()));

  // Note: In the current architecture, files are pushed automatically
  // by the remote participant after snapshot exchange. This function
  // is a placeholder for future pull-based transfer enhancement.
}

} // namespace DirShare
