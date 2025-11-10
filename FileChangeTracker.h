// FileChangeTracker.h
// Tracks file changes to prevent notification loops (FR-017, SC-011)
// When a participant receives a file change from DDS and applies it locally,
// the local file monitor will detect the change but must not republish it.

#ifndef DIRSHARE_FILE_CHANGE_TRACKER_H
#define DIRSHARE_FILE_CHANGE_TRACKER_H

#include <string>
#include <set>
#include <ace/Thread_Mutex.h>
#include <ace/Guard_T.h>

namespace DirShare {

/**
 * @class FileChangeTracker
 * @brief Thread-safe tracker for suppressing file change notifications
 *
 * This class prevents notification loops by tracking which files are being
 * updated from remote sources. When a file is being updated from a remote
 * DDS event, it is marked as "suppressed" so that the local file monitor
 * does not republish the change.
 *
 * Thread Safety: All methods are thread-safe using ACE_Thread_Mutex.
 */
class FileChangeTracker {
public:
  FileChangeTracker();
  ~FileChangeTracker();

  /**
   * @brief Mark a file path for notification suppression
   *
   * Call this BEFORE applying a remote file change to prevent the local
   * file monitor from detecting and republishing the change.
   *
   * @param path Relative file path within shared directory
   */
  void suppress_notifications(const std::string& path);

  /**
   * @brief Resume notifications for a file path
   *
   * Call this AFTER a remote file change has been fully applied.
   *
   * @param path Relative file path within shared directory
   */
  void resume_notifications(const std::string& path);

  /**
   * @brief Check if notifications are suppressed for a file
   *
   * The file monitor should call this before publishing a FileEvent.
   * If true, the change came from a remote source and should NOT be
   * republished.
   *
   * @param path Relative file path within shared directory
   * @return true if notifications are suppressed, false otherwise
   */
  bool is_suppressed(const std::string& path) const;

  /**
   * @brief Clear all suppression entries
   *
   * Useful for testing or cleanup scenarios.
   */
  void clear();

  /**
   * @brief Get count of suppressed paths (for testing/debugging)
   *
   * @return Number of currently suppressed file paths
   */
  size_t suppressed_count() const;

private:
  mutable ACE_Thread_Mutex mutex_;  // Thread-safe access
  std::set<std::string> suppressed_paths_;  // Files being updated from remote
};

} // namespace DirShare

#endif // DIRSHARE_FILE_CHANGE_TRACKER_H
