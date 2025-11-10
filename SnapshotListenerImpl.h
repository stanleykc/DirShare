#ifndef DIRSHARE_SNAPSHOT_LISTENER_IMPL_H
#define DIRSHARE_SNAPSHOT_LISTENER_IMPL_H

#include "DirShareTypeSupportImpl.h"

#include <dds/DCPS/LocalObject.h>
#include <dds/DdsDcpsSubscriptionC.h>

#include <string>

namespace DirShare {

class SnapshotListenerImpl
  : public virtual OpenDDS::DCPS::LocalObject<DDS::DataReaderListener>
{
public:
  SnapshotListenerImpl(
    const std::string& shared_dir,
    DDS::DataWriter_ptr content_writer,
    DDS::DataWriter_ptr chunk_writer);

  virtual ~SnapshotListenerImpl();

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
  DDS::DataWriter_var content_writer_;
  DDS::DataWriter_var chunk_writer_;

  // Process a received directory snapshot
  void process_snapshot(const DirectorySnapshot& snapshot);

  // Request a file from remote participant
  void request_file(const FileMetadata& metadata);
};

} // namespace DirShare

#endif // DIRSHARE_SNAPSHOT_LISTENER_IMPL_H
