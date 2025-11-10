#ifndef DIRSHARE_FILEEVENTLISTENERIMPL_H
#define DIRSHARE_FILEEVENTLISTENERIMPL_H

#include "DirShareTypeSupportImpl.h"
#include "FileChangeTracker.h"
#include <dds/DdsDcpsSubscriptionC.h>
#include <dds/DCPS/LocalObject.h>
#include <string>

namespace DirShare {

/**
 * FileEventListenerImpl: Listener for FileEvent topic
 * Handles CREATE, MODIFY, and DELETE events from remote participants
 * Integrates with FileChangeTracker to prevent notification loops (SC-011)
 */
class FileEventListenerImpl
  : public virtual OpenDDS::DCPS::LocalObject<DDS::DataReaderListener>
{
public:
  /**
   * Constructor
   * @param shared_directory Path to the shared directory
   * @param content_writer DataWriter for requesting FileContent
   * @param chunk_writer DataWriter for requesting FileChunks
   * @param change_tracker Reference to FileChangeTracker for loop prevention
   */
  FileEventListenerImpl(const std::string& shared_directory,
                        DDS::DataWriter_ptr content_writer,
                        DDS::DataWriter_ptr chunk_writer,
                        FileChangeTracker& change_tracker);

  virtual ~FileEventListenerImpl();

  // DDS DataReaderListener callbacks
  virtual void on_data_available(DDS::DataReader_ptr reader);

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

  virtual void on_subscription_matched(
    DDS::DataReader_ptr reader,
    const DDS::SubscriptionMatchedStatus& status);

  virtual void on_sample_lost(
    DDS::DataReader_ptr reader,
    const DDS::SampleLostStatus& status);

private:
  std::string shared_directory_;
  DDS::DataWriter_var content_writer_;
  DDS::DataWriter_var chunk_writer_;
  FileChangeTracker& change_tracker_;  // Reference to shared tracker for loop prevention

  /**
   * Handle CREATE event - trigger file transfer
   */
  void handle_create_event(const FileEvent& event);

  /**
   * Handle MODIFY event - check timestamp and update if newer
   */
  void handle_modify_event(const FileEvent& event);

  /**
   * Handle DELETE event - check timestamp and delete if newer
   */
  void handle_delete_event(const FileEvent& event);

  /**
   * Validate filename for security (no path traversal)
   */
  bool is_valid_filename(const std::string& filename) const;
};

} // namespace DirShare

#endif // DIRSHARE_FILEEVENTLISTENERIMPL_H
