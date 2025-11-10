// FileChangeTracker.cpp
// Implementation of notification loop prevention tracker

#include "FileChangeTracker.h"
#include <ace/Log_Msg.h>

namespace DirShare {

FileChangeTracker::FileChangeTracker()
{
  // Constructor - mutex initialized by default constructor
}

FileChangeTracker::~FileChangeTracker()
{
  // Destructor - cleanup handled by std::set destructor
}

void FileChangeTracker::suppress_notifications(const std::string& path)
{
  ACE_Guard<ACE_Thread_Mutex> guard(mutex_);

  suppressed_paths_.insert(path);

  ACE_DEBUG((LM_DEBUG,
            ACE_TEXT("FileChangeTracker: Suppressing notifications for '%C'\n"),
            path.c_str()));
}

void FileChangeTracker::resume_notifications(const std::string& path)
{
  ACE_Guard<ACE_Thread_Mutex> guard(mutex_);

  size_t erased = suppressed_paths_.erase(path);

  if (erased > 0) {
    ACE_DEBUG((LM_DEBUG,
              ACE_TEXT("FileChangeTracker: Resumed notifications for '%C'\n"),
              path.c_str()));
  } else {
    ACE_DEBUG((LM_WARNING,
              ACE_TEXT("FileChangeTracker: Attempted to resume '%C' but it was not suppressed\n"),
              path.c_str()));
  }
}

bool FileChangeTracker::is_suppressed(const std::string& path) const
{
  ACE_Guard<ACE_Thread_Mutex> guard(mutex_);

  bool suppressed = (suppressed_paths_.find(path) != suppressed_paths_.end());

  if (suppressed) {
    ACE_DEBUG((LM_DEBUG,
              ACE_TEXT("FileChangeTracker: Notifications suppressed for '%C' (remote update in progress)\n"),
              path.c_str()));
  }

  return suppressed;
}

void FileChangeTracker::clear()
{
  ACE_Guard<ACE_Thread_Mutex> guard(mutex_);

  size_t count = suppressed_paths_.size();
  suppressed_paths_.clear();

  ACE_DEBUG((LM_DEBUG,
            ACE_TEXT("FileChangeTracker: Cleared %d suppressed paths\n"),
            count));
}

size_t FileChangeTracker::suppressed_count() const
{
  ACE_Guard<ACE_Thread_Mutex> guard(mutex_);
  return suppressed_paths_.size();
}

} // namespace DirShare
