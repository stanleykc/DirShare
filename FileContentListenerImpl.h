#ifndef DIRSHARE_FILE_CONTENT_LISTENER_IMPL_H
#define DIRSHARE_FILE_CONTENT_LISTENER_IMPL_H

#include "DirShareTypeSupportImpl.h"

#include <dds/DCPS/LocalObject.h>
#include <dds/DdsDcpsSubscriptionC.h>

#include <string>

namespace DirShare {

class FileContentListenerImpl
  : public virtual OpenDDS::DCPS::LocalObject<DDS::DataReaderListener>
{
public:
  explicit FileContentListenerImpl(const std::string& shared_dir);

  virtual ~FileContentListenerImpl();

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

  // Process received file content
  void process_file_content(const FileContent& content);
};

} // namespace DirShare

#endif // DIRSHARE_FILE_CONTENT_LISTENER_IMPL_H
