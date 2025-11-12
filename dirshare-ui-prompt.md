I want to add a feature to DirShare that enables users to filter the files that subscribers receive from publishers based on the metadata of files.

I want to add to the FileMetadata the following attribute: originator (a string which uniquely identifies the originator of the file). This attribute will be defined by the user who starts the dirshare application as an argument that is required on startup. Nominally, this would likely be an email address for the originator.

Subscribers should wait to synchronize their directory initially until they have reviewed the Directory Snapshot to determine which files they choose to synchronize based on the FileMetadata.