#include "../include/Manifest.h"
#include "../include/Pack.h"
#include "../include/Utils.h"
#include <netdb.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

bool isClient()
{
    return true;
}

const char* getTaskName()
{
    static char name[128] = "" ;
   
    if (name[0] == '\0')
    { 
    strcat(name, "WTF@");
    char dirname[256];
    getcwd(dirname, sizeof(dirname));
    char* last = strrchr(dirname, '/');
    strcat(name, ++last);
    }
    return name;
}

int getConfig(char *hostOrIP, unsigned short *portno)
{
    FILE *configure = fopen(".configure", "r");
    if (!configure)
    {
        return -1;
    }
    int ninput = fscanf(configure, "%s %hu", hostOrIP, portno);
    if (ninput != 2)
    {
        return -1;
    }
    return 0;
}

int createNConnect(int argc, char *argv[])
{
    char hostnameOrIP[128];
    unsigned short portno;

    if (getConfig(hostnameOrIP, &portno) < 0)
    {
        reportError("File .configure does not exist or invalid, please run %s configure to generate a valid hostname/IP and port", argv[0]);
        return -1;
    }
    return createNConnectClientSocket(hostnameOrIP, portno);
}

int configureWTF(int argc, char *argv[])
{
    if (argc < 4)
    {
        reportError("Usage:%s configure <IP/hostname> <port>", argv[0]);
        return -1;
    }

    int port = atoi(argv[3]);
    if (port <= 0)
    {
        reportError("Invalid port number %s provided", argv[3]);
        return -1;
    }

    FILE *fconfig = fopen(".configure", "w");
    if (fconfig == NULL)
    {
        reportError("Failed to open file .configure for writing");
        return -1;
    }

    fprintf(fconfig, "%s %d\n", argv[2], port);
    fclose(fconfig);
    return 0;
}

int checkoutProject(int argc, char *argv[])
{
    if (argc < 3)
    {
        reportError("Usage: %s checkout dir-name", argv[0]);
        return -1;
    }
    struct stat st;
    if (stat(argv[2], &st) == 0)
    {
        reportError("Project %s exists, nothing to do", argv[2]);
        return -1;
    }

    int sockfd = createNConnect(argc, argv);
    encodeAndSendCommand(sockfd, CHECKOUT, argv[2]);

    char message[128];
    short cmd;
    if (readAndDecodeCommand(sockfd, &cmd, message) < 0)
    {
        return -1;
    }
    if (cmd == FAIL)
    {
        reportError("Failed to checkout project:%s, error from server:%s", argv[2], message);
        return -1;
    }
    decodeAndSaveDirectory(sockfd);
    return 0;
}

int updateProject(int argc, char *argv[])
{
    if (argc < 3)
    {
        reportError("Usage: %s update project-name", argv[0]);
        return -1;
    }
    char *project = argv[2];
    int sockfd = createNConnect(argc, argv);
    if (sockfd < 0)
    {
        return sockfd;
    }
    if (encodeAndSendCommand(sockfd, UPDATE, project) < 0)
    {
        return -1;
    }
    char message[128];
    short cmd;
    if (readAndDecodeCommand(sockfd, &cmd, message) < 0)
    {
        return -1;
    }
    if (cmd == FAIL)
    {
        reportError("Command commit failed: message from server:%s", message);
        return -1;
    }
    decodeAndSaveFile(sockfd, ".Manifest"); //save to .Manifest
    updateFromManifest("./", argv[2]);

    return 0;
}

int upgradeProject(int argc, char *argv[])
{
    if (argc < 3)
    {
        reportError("Usage: %s upgrade project-name", argv[0]);
        return -1;
    }
    char *project = argv[2];
    int sockfd = createNConnect(argc, argv);
    if (sockfd < 0)
    {
        return sockfd;
    }
    char message[128];
    char updateFilename[128];
    fullFilename(updateFilename, project, ".Update");
    if (fileExist(updateFilename) != 0)
    {
        reportError("No file .Update for this project %s", project);
        return -1;
    }
    char conflictFilename[128];
    fullFilename(conflictFilename, project, ".Conflict");
    if (fileExist(conflictFilename) == 0)
    {
        reportError("File .Conflict exists for project %s", project);
        return -1;
    }
    //somehow apply changes from updateFilename here
    if (encodeAndSendCommand(sockfd, UPGRADE, project) < 0)
    {
        return -1;
    }

    ProjectManifest *clientPM = initProjectManifest(project);
    char oper;
    char fname[128];
    char hash[65];
    int addOrModify = 0;
    int del = 0;
    FILE *update = fopen(updateFilename, "r");
    while (3 == fscanf(update, "%c%s%s\n", &oper, fname, hash))
    {
        if (oper == 'D')
        {
            //delete the file from local .Manifest file
            removeFileManifest(clientPM, fname);
            reportInfo("D %s", fname);
            ++del;
        }
        else if (oper == 'A')
        {
            reportInfo("ADD %s", fname);
            addFileManifestWithHash(clientPM, fname, hash);
            ++addOrModify;
        }
        else if (oper == 'M')
        {
            reportInfo("M %s", fname);
            //removeFileManifest(clientPM, fname);
            //addFileManifestWithHash(clientPM,fname,hash);
            updateFileManifestWithHash(clientPM, fname, hash);
            ++addOrModify;
        }
        else if (oper == 'V')
        {
            //the version of server Manifest, will be copied to client
            clientPM->version = atoi(fname);
        }
    }
    fclose(update);

    if (addOrModify == 0 && del == 0)
    {
        unlink(updateFilename);
        reportInfo("Empty .Update file, nothing to do");
        return 0;
    }
    else if (addOrModify != 0)
    {
        reportInfo("Send file .Update to server");

        if (encodeAndSendCommand(sockfd, ONEFILE, updateFilename) < 0)
        {
            return -1;
        }

        if (encodeAndSendFile(sockfd, updateFilename) < 0)
        {
            return -1;
        }

        if (decodeAndSaveDirectory(sockfd) < 0)
        {
            return -1;
        }
    }
    reportInfo("Update the client's Manifest file");
    persistProjectManifest(clientPM);
    unlink(updateFilename);
    return 0;
}

int commitProject(int argc, char *argv[])
{
    if (argc < 3)
    {
        reportError("Usage: %s commit project-name", argv[0]);
        return -1;
    }
    char *project = argv[2];
    char updateFilename[128];
    fullFilename(updateFilename, project, ".Update");
    if (fileExist(updateFilename) == 0)
    {
        reportError("File .Update exist for project %s", project);
        return -1;
    }
    char conflictFilename[128];
    fullFilename(conflictFilename, project, ".Conflict");
    if (fileExist(conflictFilename) == 0)
    {
        reportError("File .Conflict exist for project %s", project);
        return -1;
    }
    int sockfd = createNConnect(argc, argv);
    if (sockfd < 0)
    {
        return sockfd;
    }
    if (encodeAndSendCommand(sockfd, COMMIT, project) < 0)
    {
        return -1;
    }
    char message[128];
    short cmd;
    if (readAndDecodeCommand(sockfd, &cmd, message) < 0)
    {
        return -1;
    }
    if (cmd == FAIL)
    {
        reportError("Command commit failed: message from server:%s", message);
        return -1;
    }
    decodeAndSaveFile(sockfd, ".Manifest"); //save to .Manifest
    if (compare2Manifest("./", project) != 0)
    {
        return -1;
    }
    char commitFile[128];
    fullFilename(commitFile, project, ".Commit");
    if (fileExist(commitFile) == 0)
    {
        reportInfo("Send .Commit file to server");
        encodeAndSendCommand(sockfd, ONEFILE, commitFile);
        encodeAndSendFile(sockfd, commitFile);
    }
    else
    {
        reportInfo("File .Commit does not exist");
    }
    return 0;
}

int pushProject(int argc, char *argv[])
{
    if (argc < 3)
    {
        reportError("Usage: %s commit project-name", argv[0]);
        return -1;
    }
    int sockfd = createNConnect(argc, argv);
    if (sockfd < 0)
    {
        return sockfd;
    }
    char *project = argv[2];
    char commitFilename[128];
    fullFilename(commitFilename, project, ".Commit");
    FILE *commit = fopen(commitFilename, "r");
    if (commit == NULL)
    {
        reportError("File %s does not exist, please run commit first", commitFilename);
        return -1;
    }
    ProjectManifest *clientPM = initProjectManifest(project);
    char operation;
    char filename[128];
    char hash[128];
    VectorStr vec;
    str_init(&vec);
    str_append(&vec, commitFilename);
    char buffer[128];
    int n = fscanf(commit, "%c%s%s\n", &operation, filename, hash);
    while (n == 2 || n == 3)
    {
        if (operation == 'A' || operation == 'M')
        {
            char fullname[128];
            fullFilename(fullname, project, filename);
            str_append(&vec, fullname);
            reportInfo("Add file %s to send", fullname);
            if (operation == 'M')
            {
                updateFileManifestWithHash(clientPM, filename, hash);
            }
        }
        n = fscanf(commit, "%c%s%s\n", &operation, filename, hash);
    }
    encodeAndSendCommand(sockfd, PUSH, project);
    encodeAndSendFiles(sockfd, &vec);
    short cmd;
    char message[128];
    readAndDecodeCommand(sockfd, &cmd, message);
    reportInfo("Response from the server:%s", message);
    if (cmd == FAIL)
    {
        return -1;
    }
    unlink(commitFilename);
    ++clientPM->version;
    persistProjectManifest(clientPM);
    return 0;
}

int createProject(int argc, char *argv[])
{
    if (argc < 3)
    {
        reportError("Usage: %s create project-name", argv[0]);
        return -1;
    }
    int sockfd = createNConnect(argc, argv);
    if (sockfd < 0)
    {
        return sockfd;
    }
    char *project = argv[2];
    if (encodeAndSendCommand(sockfd, CREATE, project) < 0)
    {
        return -1;
    }
    char message[128];
    short cmd;
    if (readAndDecodeCommand(sockfd, &cmd, message) < 0)
    {
        return -1;
    }
    if (cmd == FAIL)
    {
        reportError("Server Failed to create project, error from server:%s", message);
        return -1;
    }
    if (decodeAndSaveDirectory(sockfd) < 0)
    {
        return -1;
    }
    return 0;
}

int destroyProject(int argc, char *argv[])
{
    if (argc < 3)
    {
        reportError("Usage: %s destroy project-name", argv[0]);
        return -1;
    }
    int sockfd = createNConnect(argc, argv);
    if (sockfd < 0)
    {
        return sockfd;
    }
    char *project = argv[2];
    if (encodeAndSendCommand(sockfd, DESTROY, project) < 0)
    {
        return -1;
    }
    char message[128];
    short cmd;
    if (readAndDecodeCommand(sockfd, &cmd, message) < 0)
    {
        return -1;
    }
    if (cmd == FAIL)
    {
        reportError("Server Failed to destroy project %s, error from server:%s", project, message);
        return -1;
    }

    reportInfo("Project %s destroyed", project);
    return 0;
}

int addFile(int argc, char *argv[])
{
    if (argc < 4)
    {
        reportError("Usage:%s add project-name file-name", argv[0]);
        return -1;
    }
    char *project = argv[2];
    char *filename = argv[3];
    char fullname[128];
    fullFilename(fullname, project, filename);
    struct stat st;
    if (stat(fullname, &st) != 0)
    {
        reportError("File %s does not exists", fullname);
        return -1;
    }

    ProjectManifest *pm = initProjectManifest(project);
    if (!pm)
    {
        reportError("Project %s does not exist", project);
        return -1;
    }

    if (findFileManifest(pm, filename))
    {
        reportError("File %s exist in project %s", filename, project);
        return -1;
    }

    addFileManifest(pm, filename);
    persistProjectManifest(pm);
    return 0;
}

int removeFile(int argc, char *argv[])
{
    if (argc < 4)
    {
        reportError("Usage:%s add project-name file-name", argv[0]);
        return -1;
    }
    char *project = argv[2];
    char *filename = argv[3];

    char fullname[128];
    fullFilename(fullname, project, filename);
    struct stat st;
    if (stat(fullname, &st) != 0)
    {
        reportError("File %s does not exists", fullname);
        return -1;
    }

    ProjectManifest *pm = initProjectManifest(project);
    if (!pm)
    {
        reportError("Project %s does not exist", project);
        return -1;
    }

    if (removeFileManifest(pm, filename) < 0)
    {
        return -1;
    }
    return persistProjectManifest(pm);
}

int currentversionProject(int argc, char *argv[])
{
    if (argc < 3)
    {
        reportError("Usage: %s currentversion project-name", argv[0]);
        return -1;
    }
    int sockfd = createNConnect(argc, argv);
    if (sockfd < 0)
    {
        return sockfd;
    }
    char *project = argv[2];
    if (encodeAndSendCommand(sockfd, CURRENTVERSION, project) < 0)
    {
        return -1;
    }
    char message[128];
    short cmd;
    if (readAndDecodeCommand(sockfd, &cmd, message) < 0)
    {
        return -1;
    }
    if (cmd == FAIL)
    {
        reportError("Server Failed to get current version of the project, error from server:%s", message);
        return -1;
    }
    if (decodeAndSaveFile(sockfd, ".Manifest") < 0)
    {
        return -1;
    }

    displayCurrentversion(project);
    return 0;
}

int historyProject(int argc, char *argv[])
{
    if (argc < 3)
    {
        reportError("Usage: %s history project-name", argv[0]);
        return -1;
    }
    int sockfd = createNConnect(argc, argv);
    if (sockfd < 0)
    {
        return sockfd;
    }
    char *project = argv[2];
    if (encodeAndSendCommand(sockfd, HISTORY, project) < 0)
    {
        return -1;
    }
    char message[128];
    short cmd;
    if (readAndDecodeCommand(sockfd, &cmd, message) < 0)
    {
        return -1;
    }
    if (cmd == FAIL)
    {
        reportError("Server Failed to get history of the project, error from server:%s", message);
        return -1;
    }
    if (decodeAndSaveFile(sockfd, "history") < 0)
    {
        return -1;
    }

    displayHistory(project);
    return 0;
}

int rollbackProject(int argc, char *argv[])
{
    if (argc < 4)
    {
        reportError("Usage: %s rollback project-name", argv[0]);
        return -1;
    }
    int sockfd = createNConnect(argc, argv);
    if (sockfd < 0)
    {
        return sockfd;
    }
    char *project = argv[2];
    char command[128];
    snprintf(command, sizeof(command), "%s %s", project, argv[3]);
    if (encodeAndSendCommand(sockfd, ROLLBACK, command) < 0)
    {
        return -1;
    }
    char message[128];
    short cmd;
    if (readAndDecodeCommand(sockfd, &cmd, message) < 0)
    {
        return -1;
    }
    if (cmd == FAIL)
    {
        reportError("Server Failed to rollback the project, error from server:%s", message);
        return -1;
    }
    else
    {
        reportInfo("Success: from server received received: %s", message);
    }
    return 0;
}

int processCommand(int argc, char *argv[])
{
    if (argc < 2)
    {
        reportError("Usage: %s configure|create|... missing arguments", argv[0]);
        return -1;
    }
    if (!strcmp(argv[1], "configure"))
    {
        return configureWTF(argc, argv);
    }
    else if (!strcmp(argv[1], "checkout"))
    {
        return checkoutProject(argc, argv);
    }
    else if (!strcmp(argv[1], "update"))
    {
        return updateProject(argc, argv);
    }
    else if (!strcmp(argv[1], "upgrade"))
    {
        return upgradeProject(argc, argv);
    }
    else if (!strcmp(argv[1], "commit"))
    {
        return commitProject(argc, argv);
    }
    else if (!strcmp(argv[1], "push"))
    {
        return pushProject(argc, argv);
    }
    else if (!strcmp(argv[1], "create"))
    {
        return createProject(argc, argv);
    }
    else if (!strcmp(argv[1], "destroy"))
    {
        return destroyProject(argc, argv);
    }
    else if (!strcmp(argv[1], "add"))
    {
        return addFile(argc, argv);
    }
    else if (!strcmp(argv[1], "remove"))
    {
        return removeFile(argc, argv);
    }
    else if (!strcmp(argv[1], "currentversion"))
    {
        return currentversionProject(argc, argv);
    }
    else if (!strcmp(argv[1], "history"))
    {
        return historyProject(argc, argv);
    }
    else if (!strcmp(argv[1], "rollback"))
    {
        return rollbackProject(argc, argv);
    }
    else
    {
        reportError("unknown command %s", argv[1]);
        return -1;
    }
}

int main(int argc, char *argv[])
{
    srand(time(NULL));
    char buffer[1024];
    memset(buffer, '\0', sizeof(buffer));
    int idx = 1;
    for (; idx < argc; idx++)
    {
        strcat(buffer, argv[idx]);
        strcat(buffer, " ");
    } 
    reportInfo("Start processing %s",buffer);
    int status = processCommand(argc, argv);
    reportInfo("End processing %s",buffer);
    return status;
}
