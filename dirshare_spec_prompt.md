DirShare is a simple example that demonstrates how to share files between DDS participants. This example consists of a publisher that shares files from a specified directory and a subscriber that receives and saves those files to a local directory.

It supports establishing a directory sharing session, during which the folder in which the DirShare application/utility on your local workstation to share with the remote participants who have also run on their systems. Changes to the folder on either the remote desktop or your workstation are reflected on both machines for the duration of the session. So whenever a file is created, modified, or deleted in the shared directory on one side, the change is propagated to the other side. If a file is modified simultaneously on both sides, the last modification takes precedence.

It should manage file transfers efficiently, ensuring that only changed files are transmitted to minimize bandwidth usage.

It should support the transfer of metadata associated with the files, such as file names, sizes, and timestamps as well as the file contents. This will allow for future features where users can filter or sort files based on their metadata before synchronizing them.
