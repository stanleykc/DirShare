#include "FileContentListenerImpl.h"
#include "FileUtils.h"
#include "Checksum.h"

#include <ace/Log_Msg.h>
#include <ace/OS_NS_sys_stat.h>

namespace DirShare {

FileContentListenerImpl::FileContentListenerImpl(const std::string& shared_dir,
                                                   FileChangeTracker& change_tracker)
  : shared_dir_(shared_dir)
  , change_tracker_(change_tracker)
{
}

FileContentListenerImpl::~FileContentListenerImpl()
{
}

void FileContentListenerImpl::on_requested_deadline_missed(
  DDS::DataReader_ptr,
  const DDS::RequestedDeadlineMissedStatus&)
{
}

void FileContentListenerImpl::on_requested_incompatible_qos(
  DDS::DataReader_ptr,
  const DDS::RequestedIncompatibleQosStatus&)
{
}

void FileContentListenerImpl::on_sample_rejected(
  DDS::DataReader_ptr,
  const DDS::SampleRejectedStatus&)
{
}

void FileContentListenerImpl::on_liveliness_changed(
  DDS::DataReader_ptr,
  const DDS::LivelinessChangedStatus&)
{
}

void FileContentListenerImpl::on_subscription_matched(
  DDS::DataReader_ptr,
  const DDS::SubscriptionMatchedStatus&)
{
}

void FileContentListenerImpl::on_sample_lost(
  DDS::DataReader_ptr,
  const DDS::SampleLostStatus&)
{
}

void FileContentListenerImpl::on_data_available(DDS::DataReader_ptr reader)
{
  FileContentDataReader_var content_reader =
    FileContentDataReader::_narrow(reader);

  if (!content_reader) {
    ACE_ERROR((LM_ERROR,
               ACE_TEXT("ERROR: %N:%l: FileContentListenerImpl::on_data_available() - ")
               ACE_TEXT("failed to narrow DataReader!\n")));
    return;
  }

  FileContent content;
  DDS::SampleInfo info;

  DDS::ReturnCode_t status = content_reader->take_next_sample(content, info);

  if (status == DDS::RETCODE_OK) {
    if (info.valid_data) {
      ACE_DEBUG((LM_INFO,
                 ACE_TEXT("(%P|%t) Received FileContent: %C (%Q bytes)\n"),
                 content.filename.in(),
                 content.size));

      process_file_content(content);
    }
  } else if (status != DDS::RETCODE_NO_DATA) {
    ACE_ERROR((LM_ERROR,
               ACE_TEXT("ERROR: %N:%l: FileContentListenerImpl::on_data_available() - ")
               ACE_TEXT("take_next_sample failed: %d\n"),
               status));
  }
}

void FileContentListenerImpl::process_file_content(const FileContent& content)
{
  std::string filename = content.filename.in();
  std::string full_path = shared_dir_ + "/" + filename;

  // Check if file exists and compare timestamps for MODIFY case
  if (file_exists(full_path)) {
    unsigned long long local_timestamp_sec;
    unsigned long local_timestamp_nsec;
    if (get_file_mtime(full_path, local_timestamp_sec, local_timestamp_nsec)) {
      // Compare remote timestamp with local timestamp
      bool remote_is_newer = false;
      if (content.timestamp_sec > local_timestamp_sec) {
        remote_is_newer = true;
      } else if (content.timestamp_sec == local_timestamp_sec &&
                 content.timestamp_nsec > local_timestamp_nsec) {
        remote_is_newer = true;
      }

      if (!remote_is_newer) {
        ACE_DEBUG((LM_INFO,
                   ACE_TEXT("(%P|%t) Local file is newer or same, ignoring FileContent for: %C\n"),
                   filename.c_str()));
        // Resume notifications even when rejecting update (SC-011: prevent permanent suppression)
        change_tracker_.resume_notifications(filename);
        return;
      }

      ACE_DEBUG((LM_INFO,
                 ACE_TEXT("(%P|%t) Remote file is newer, updating local file: %C\n"),
                 filename.c_str()));
    }
  }

  // Validate metadata: size matches actual data length
  if (content.size != content.data.length()) {
    ACE_ERROR((LM_ERROR,
               ACE_TEXT("ERROR: %N:%l: Size mismatch for file %C\n")
               ACE_TEXT("  Metadata size: %Q, Actual data length: %u\n"),
               filename.c_str(),
               content.size,
               content.data.length()));
    // Resume notifications on error (SC-011: prevent permanent suppression)
    change_tracker_.resume_notifications(filename);
    return;
  }

  // Verify checksum
  if (content.data.length() > 0) {
    uint32_t computed_checksum = compute_checksum(
      reinterpret_cast<const uint8_t*>(content.data.get_buffer()),
      content.data.length());

    if (computed_checksum != content.checksum) {
      ACE_ERROR((LM_ERROR,
                 ACE_TEXT("ERROR: %N:%l: Checksum mismatch for file %C\n")
                 ACE_TEXT("  Expected: 0x%08X, Computed: 0x%08X\n"),
                 filename.c_str(),
                 content.checksum,
                 computed_checksum));
      // Resume notifications on error (SC-011: prevent permanent suppression)
      change_tracker_.resume_notifications(filename);
      return;
    }
  }

  // Write file content
  if (!write_file(full_path,
                  reinterpret_cast<const uint8_t*>(content.data.get_buffer()),
                  content.data.length())) {
    ACE_ERROR((LM_ERROR,
               ACE_TEXT("ERROR: %N:%l: Failed to write file: %C\n"),
               full_path.c_str()));
    // Resume notifications on error (SC-011: prevent permanent suppression)
    change_tracker_.resume_notifications(filename);
    return;
  }

  // Preserve timestamp
  ACE_DEBUG((LM_DEBUG,
             ACE_TEXT("(%P|%t) Preserving timestamp for %C: %Q.%09u\n"),
             filename.c_str(),
             content.timestamp_sec,
             content.timestamp_nsec));

  if (!set_file_mtime(full_path, content.timestamp_sec, content.timestamp_nsec)) {
    ACE_ERROR((LM_WARNING,
               ACE_TEXT("WARNING: %N:%l: Failed to set timestamp for file: %C\n"),
               full_path.c_str()));
    // Don't fail the operation - file was written successfully
  } else {
    // Verify timestamp was preserved correctly
    unsigned long long verified_sec;
    unsigned long verified_nsec;
    if (get_file_mtime(full_path, verified_sec, verified_nsec)) {
      ACE_DEBUG((LM_DEBUG,
                 ACE_TEXT("(%P|%t) Timestamp preserved for %C: original=%Q.%09u, actual=%Q.%09u\n"),
                 filename.c_str(),
                 content.timestamp_sec,
                 content.timestamp_nsec,
                 verified_sec,
                 verified_nsec));
    }
  }

  ACE_DEBUG((LM_INFO,
             ACE_TEXT("(%P|%t) Successfully wrote file: %C (%Q bytes, checksum: 0x%08X)\n"),
             filename.c_str(),
             content.size,
             content.checksum));

  // Resume notifications for this file (SC-011: prevent notification loop)
  change_tracker_.resume_notifications(filename);
  ACE_DEBUG((LM_DEBUG,
             ACE_TEXT("(%P|%t) Resumed notifications for file: %C\n"),
             filename.c_str()));
}

} // namespace DirShare
