# WTF (Where is the Files)

WTF (Where is the Files) is a version control system that has both server-side and client-side components. The server maintains repositories for projects, each consisting of files and a .Manifest to record the version and a sha-256 digest. The server holds the master copy of the data, and client copies are synchronized with it periodically. Files/updates can go from client to server and vice versa. Unlike git, WTF does not have the concept of branches and can only update files using Fast Forward (as defined in git) in case of no conflict.

## Introduction (of concepts)

- **project**: Similar to git.
- **repository**: Similar to git.
- **.Manifest**: A metadata file listing all paths to all files in a project, their state (version), and a hash of all the contents of each file.
- **history**: A list of all updates/changes made to a given project since creation (maintained at the repository, but can be requested by clients).
- **rollback**: Change the current version of a project in the repository to be a previous version.
- **commit/push**: Upload changes you made to a project locally to the repository, updating its current version.
- **checkout**: Download the current version of a project in the repository.
- **update**: Download from the repository only the files in a project that are newer than your local copy.

## WTF client

The WTF client program takes as command-line arguments a WTF command and a number of arguments. Most commands require a project name first, some need only the project name. The first command the client is given should always be a configure command where the hostname or IP address of the machine on which the server is located as well as the port number where it is listening are command-line arguments. After a configure is done, based on the command it is given, the client may send different things to the WTF server. The client program can take only one command per invocation. The client's job is to maintain a Manifest; a list of all files currently considered to be part of the project and to verify that list with the server when asked. Most commands that result in files being sent or received to or from the server have two halves; a command of preparation to get ready for an operation and then a command of execution to do the operation:

- **update**: Get the server's .Manifest and compare all entries in it with the client's .Manifest and see what changes need to be made to the client's files to bring them up to the same version as the server, and write out a .Update file recording all those changes that need to be made.
- **upgrade**: Make all the changes listed in the .Update to the client side.
- **commit**: Get the server's .Manifest and compare all entries in it with the client's .Manifest and find out which files the client has that are newer versions than the ones on the server, or the server does not have, and write out a .Commit recording all the changes that need to be made.
- **push**: Make all the changes listed in the .Commit to the server side.

## WTF server

The WTF server program firstly needs to be multithreaded, as it needs to serve potentially any number of clients at once. It should spawn a new client service thread whenever it gets a new connection request. It should not do any client communication in the same execution context that listens for new connections.

Since there will be multiple threads trying to access the files in the repository at the same time, you should have a mutex per project to control access to it. Be sure to lock the mutex whenever reading or writing information or files from or to a project. You do not want to, for instance, send a .Manifest to a client while you're adding a file to it so that the .Manifest sent would be out of date the moment it is sent. Be careful not to deadlock your server's mutexes.

When being started, the server takes a single command-line argument, a port number to listen on.

./WTFserver 9123

## WTF Client Commands
-------------------

### `./WTF configure <IP/hostname> <port>`
It will save the information in a file `.configure` to be used by future commands.

### The following 3 commands will apply changes from the server side to the client side:

#### `./WTF checkout <project name>`
It will checkout a project (including `.Manifest` of the project and all files).

#### `./WTF update <project name>`
It will compare local files of the project with remote ones, and write a `.update` file if no conflict; the file will indicate all the changes the server went through.

#### `./WTF upgrade <project name>`
It will upgrade the local version to the same as the server has, and the `.update` file will be deleted.

### The following 2 commands will send local changes from the client side to the server side:

#### `./WTF commit <project name>`
It will compare the local `.Manifest` with the server's, and find out differences; if there is no conflict, a `.Commit` file will be written locally.

#### `./WTF push <project name>`
It will send all the changes, including `.Manifest` and all files to the server. All files for the project will be compressed with `tar` and `gz` commands so that only one file needs to be sent.

### Some more commands:

#### `./WTF create <project name>`
It will ask the server to create a project with the name.

#### `./WTF destroy <project name>`
It will ask the server to destroy the project with the name.

#### `./WTF add <project name> <filename>`
It will add a file with the name locally to the project; the file must have been created already. The `.Manifest` file will be updated.

#### `./WTF remove <project name> <filename>`
It will remove the file from the project and update file `.Manifest`.

#### `./WTF currentversion <project name>`
It will ask the current version from the server and display it.

#### `./WTF history <project name>`
It will ask the history of the project from the server and display it.

#### `./WTF rollback <project name> <version>`
It will rollback the project to the previous version.

Implementation
--------------

First, library code such as `encodeAndSendCommand()`, `readAndDecodeCommand()`, `encodeAndSendFiles()`, `decodeAndSaveDirectory()` are developed. Then client/server code just call these functions to manipulate data and send data across socket; then the other end will decode the data and duplicate the files and `.Manifest` from the other end.
"""
