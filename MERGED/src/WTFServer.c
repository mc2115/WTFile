/* A simple server in the internet domain using TCP
   The port number is passed as an argument */
#include "../include/Manifest.h"
#include "../include/Pack.h"
#include "../include/VectorProject.h"
#include "../include/VectorStr.h"
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

bool isClient()
{
    return false;
}

const char *getTaskName()
{
    return "WTFServer";
}

VectorProject projects;

int createServerSocket(int portno)
{
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        reportError("ERROR opening socket");
        return sockfd;
    }

    struct sockaddr_in serv_addr;
    bzero((char *)&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);
    if (bind(sockfd, (struct sockaddr *)&serv_addr,
             sizeof(serv_addr)) < 0)
    {
        reportError("ERROR on binding");
        close(sockfd);
        return -1;
    }

    return sockfd;
}

int createProject(char *project, char message[], int mlen)
{
    if (mkdir(project, 0777) == 0)
    {
        char cmd[1024];
        snprintf(cmd, sizeof(cmd), "echo 0 %s 0000 > %s/.Manifest", project, project);
        if (system(cmd) < 0)
        {
            char message[256];
            snprintf(message, mlen, "Failed to create .Manifest file under %s", project);
            return -1;
        }
        return 0;
    }
    else
    {
        snprintf(message, mlen, "Failed to create directory:%s %d=%s", project, errno, strerror(errno));
        return -1;
    }
}

int destroyProject(char *project, char message[], int mlen)
{
    struct stat st;
    if (stat(project, &st) == 0)
    {
        char cmd[256];
        snprintf(cmd, sizeof(cmd), "rm -rf %s", project);
        if (system(cmd) < 0)
        {
            snprintf(message, mlen, "Failed to destroy project %d=%s", errno, strerror(errno));
            return -1;
        }
    }
    else
    {
        snprintf(message, mlen, "project %s does not exist", project);
        return -1;
    }
    return 0;
}

int processCmd(int sockfd, Project *pstruc, int cmd, char *project)
{

    if (cmd == CHECKOUT)
    {
        struct stat st;
        if (stat(project, &st) == 0)
        {
            encodeAndSendCommand(sockfd, CHECKOUT, project);
            ProjectManifest *pm = initProjectManifest(project);

            VectorStr filenames;
            str_init(&filenames);
            int idx = 0;
            for (; idx < filemanifest_size(&pm->vector); idx++)
            {
                char fullname[128];
                fullFilename(fullname, project, filemanifest_getData(&pm->vector, idx)->filename);
                str_append(&filenames, fullname);
            }
            char manifestName[128];
            fullFilename(manifestName, project, ".Manifest");
            str_append(&filenames, manifestName);

            encodeAndSendFiles(sockfd, &filenames);
            return 0;
        }
        else
        {
            char message[256];
            snprintf(message, sizeof(message), "Project %s does not exist", project);
            encodeAndSendCommand(sockfd, FAIL, message);
            return -1;
        }
    }
    else if (cmd == UPDATE)
    {
        struct stat st;
        if (stat(project, &st) == 0)
        {
            char manifestFile[128];
            snprintf(manifestFile, sizeof(manifestFile), "%s/.Manifest", project);
            encodeAndSendCommand(sockfd, ONEFILE, manifestFile);
            encodeAndSendFile(sockfd, manifestFile);
            return 0;
        }
        else
        {
            reportError("Project %s not exist", project);
            char message[256];
            snprintf(message, sizeof(message), "Project %s does not exist", project);
            encodeAndSendCommand(sockfd, FAIL, message);
            return -1;
        }
    }
    else if (cmd == UPGRADE)
    {
        short cmd2;
        char filename[128];
        if (readAndDecodeCommand(sockfd, &cmd2, filename) < 0)
        {
            return -1;
        }
        if (cmd2 != ONEFILE)
        {
            return -1;
        }
        if (decodeAndSaveFile(sockfd, ".Upgrade") < 0)
        {
            return -1;
        }


        char operation;
        char hash[128];
        VectorStr vec;
        str_init(&vec);
        char buffer[128];
        FILE *upgrade = fopen(".Upgrade", "r");
        int n = fscanf(upgrade, "%c%s%s\n", &operation, filename, hash);
        while (n == 2 || n == 3)
        {
            if (operation == 'A' || operation == 'M')
            {
                char fullname[128];
                fullFilename(fullname, project, filename);
                str_append(&vec, fullname);
                reportInfo("Add file %s to send", fullname);
            }
            n = fscanf(upgrade, "%c%s%s\n", &operation, filename, hash);
        }
        encodeAndSendFiles(sockfd, &vec);
        fclose(upgrade);
    }
    else if (cmd == COMMIT)
    {
        struct stat st;
        if (stat(project, &st) == 0)
        {
            reportInfo("project %s exist",project);
            char manifestFile[128];
            snprintf(manifestFile, sizeof(manifestFile), "%s/.Manifest", project);
            encodeAndSendCommand(sockfd, ONEFILE, manifestFile);
            encodeAndSendFile(sockfd, manifestFile);
            short cmd;
            char message[128];
            if (readAndDecodeCommand(sockfd, &cmd, message) < 0)
            {
                return -1;
            }
            if (cmd == ONEFILE)
            {
                //printf("ONEFILE CMD:%d MSG:%s\n", cmd, message);
                int randInt = rand();
                char filename[128];
                snprintf(filename, sizeof(filename), "%d.Commit", randInt);
                decodeAndSaveFile(sockfd, filename);
                project_addCommitFile(pstruc, filename);
                return 0;
            }
            return -1;
        }
        else
        {
            reportError("Project %s not exist", project);
            char message[256];
            snprintf(message, sizeof(message), "Project %s does not exist", project);
            encodeAndSendCommand(sockfd, FAIL, message);
            return -1;
        }
    }
    else if (cmd == PUSH)
    {
        ProjectManifest *pm = initProjectManifest(project);
        if (pm == NULL)
        {
            char message[128];
            snprintf(message, sizeof(message), "Project %s does not exist", project);
            reportError("Project %s does not exist", project);
            encodeAndSendCommand(sockfd, FAIL, message);
            return -1;
        }
        char cmd[128];
        snprintf(cmd, sizeof(cmd), "cp -r %s %s.%d;", project, project, pm->version);
        if (system(cmd) < 0)
        {
            char message[128];
            snprintf(message, sizeof(message), "Failed to execute command:%s\n", cmd);
            encodeAndSendCommand(sockfd, FAIL, message);
            return -1;
        }
        decodeAndSaveFiles(sockfd);
        //compare the 2 .Commit files
        char commitFilename[128];
        fullFilename(commitFilename, project, ".Commit");
        int idx = 0;
        bool match = false;
        for (; idx < str_size(&pstruc->commitFiles); idx++)
        {
            char *pname = str_getData(&pstruc->commitFiles, idx);
            if (compare2Files(pname, commitFilename) == 0)
            {
                match = true;
                break;
            }
        }
        if (!match)
        {
            char cmd2[128];
            snprintf(cmd2, sizeof(cmd2), "rm -rf %s; mv -f %s.%d %s", project, project, pm->version, project);
            system(cmd2); //doing best to recover
            encodeAndSendCommand(sockfd, FAIL, "No pre-existing commit files match the one received, please re-commit");
            return -1;
        }
        else
        {
            idx = 0;
            for (; idx < str_size(&pstruc->commitFiles); idx++)
            {
                char *pname = str_getData(&pstruc->commitFiles, idx);
                unlink(pname);
            }
            str_reset(&pstruc->commitFiles);
        } //delete all .Commit files
        reportInfo("COMPARED 2 commit files, they match");

        snprintf(cmd, sizeof(cmd), "tar cfz %s.%d.tar.gz %s.%d && rm -rf %s.%d", project, pm->version, project, pm->version, project, pm->version);
        system(cmd);

        FILE *commit = fopen(commitFilename, "r");
        FileManifest fm;
        char operation;
        int count = 0;
        char buffer[1024];
        while (fgets(buffer, sizeof(buffer), commit))
        {
            buffer[strlen(buffer) - 1] = '\0';
            if (buffer[0] == 'D')
            {
                char delcmd[128];
                snprintf(delcmd, sizeof(delcmd), "rm %s/%s", project, &buffer[2]);
                system(delcmd);
                removeFileManifest(pm, &buffer[2]);
            }
            else
            {
                int num = sscanf(buffer, "%c%s%s\n", &operation, fm.filename, fm.hash);
                updatePMWithFile(pm, &fm);
                reportInfo("Processing commit: %c %s %s", buffer[0], fm.filename, fm.hash);
            }
            ++count;
        }
        if (count > 0)
        {
            ++pm->version;
        }
        persistProjectManifest(pm);
        encodeAndSendCommand(sockfd, SUCCESS, "Push Success");
    }
    else if (cmd == CREATE)
    {
        char message[256];
        if (createProject(project, message, sizeof(message)) < 0)
        {
            encodeAndSendCommand(sockfd, FAIL, message);
            return -1;
        }
        else
        {
            encodeAndSendCommand(sockfd, CREATE, "SUCCESS!");
            encodeAndSendDirectory(sockfd, project);
            return 0;
        }
    }
    else if (cmd == DESTROY)
    {
        char message[256];
        if (destroyProject(project, message, sizeof(message)) < 0)
        {
            encodeAndSendCommand(sockfd, FAIL, message);
            return -1;
        }
        else
        {
            encodeAndSendCommand(sockfd, SUCCESS, "Success!");
            return 0;
        }
    }
    else if (cmd == CURRENTVERSION)
    {
        encodeAndSendCommand(sockfd, ONEFILE, "CurrentVersionManifest");
        char manifestFile[128];
        fullFilename(manifestFile, project, ".Manifest");
        encodeAndSendFile(sockfd, manifestFile);
    }
    else if (cmd == HISTORY)
    {
        char command[1024];
        ProjectManifest *pm = initProjectManifest(project);
        if (pm == NULL)
        {
            encodeAndSendCommand(sockfd, FAIL, "History");
            return -1;
        }
        snprintf(command, sizeof(command), "for file in %s.*.tar.gz; do dir=$(echo $file|cut -f 1-2 -d '.'); ver=$(echo $dir |cut -f2 -d '.'); if [ $ver == 0 ]; then continue; fi; tar xzf $file $dir/.Commit; done", project);
        system(command);
        //snprintf(command, sizeof(command), "(for dir in %s.*; do if ![-f $dir/.Commit]; then continue; echo Version $(echo $dir |cut -f2 -d '.'); cat $dir/.Commit|cut -f 1-2 -d ' ';done; ) > history",project);
        snprintf(command, sizeof(command), "(for dir in %s.*; do if [ ! -f $dir/.Commit ]; then continue; fi; ver=$(echo $dir |cut -f2 -d '.'); if [ $ver == 0 ];then continue; fi;echo Version $ver;cat $dir/.Commit|cut -f 1-2 -d ' ';done; echo Version %d; cat %s/.Commit | cut -f 1-2 -d ' ') > history", project, pm->version, project);
        //printf("Command:%s\n", command);
        if (system(command) < 0)
        {
            encodeAndSendCommand(sockfd, FAIL, "History");
        }
        else
        {
            encodeAndSendCommand(sockfd, ONEFILE, "History");
            encodeAndSendFile(sockfd, "history");
        }
        unlink("history");
        snprintf(command, sizeof(command), "for file in %s.*; do dir=$(echo $file|cut -f 1-2 -d '.'); rm -rf $dir; done", project);
        system(command);
    }
    else if (cmd == ROLLBACK)
    {
        char realProject[128];
        short version;
        sscanf(project, "%s%hd", realProject, &version);
        ProjectManifest *pm = initProjectManifest(realProject);
        if (pm == NULL)
        {
            return -1;
        }
        if (version >= pm->version)
        {
            char message[128];
            snprintf(message, sizeof(message), "Given version %d, current version %d, nothing to rollback", version, pm->version);
            encodeAndSendCommand(sockfd, SUCCESS, message);
        }
        else
        {
            short save_ver = version;
            reportInfo("Rollback current version:%d rollback version:%d", pm->version, version);
            version++;
            for (; version < pm->version; version++)
            {
                char command[128];
                snprintf(command, sizeof(command), "rm -rf %s.%d*", realProject, version);
                //printf("CMD:%s\n", command);
                system(command);
            }

            char command[128];
            snprintf(command, sizeof(command), "rm -rf %s", realProject);
            //printf("CMD:%s\n", command);
            system(command);
            snprintf(command, sizeof(command), "tar xvf %s.%d.tar.gz && rm %s.%d.tar.gz", realProject, save_ver, realProject, save_ver);
            //printf("CMD:%s\n", command);
            system(command);
            snprintf(command, sizeof(command), "mv %s.%d %s", realProject, save_ver, realProject);
            //printf("CMD:%s\n", command);
            system(command);
            encodeAndSendCommand(sockfd, SUCCESS, "Done Rollback");
        }
    }
    return 0;
}

void *processConnection(void *ptr)
{
    int newsockfd = *((int *)ptr);
    short cmd;
    char project[128];
    int status = readAndDecodeCommand(newsockfd, &cmd, project);
    if (status != 0)
    {
        return NULL;
    }
    reportInfo("Received message-----%s", msgType(cmd));
    if (status == 0)
    {
        Project *p = project_getOrCreate(&projects, project);
        pthread_mutex_lock(&p->lock);
        status = processCmd(newsockfd, p, cmd, project);
        pthread_mutex_unlock(&p->lock);
    }
    close(newsockfd);
    reportInfo("Finished processing message-----%s", msgType(cmd));
    return NULL;
}

void cleanup()
{
    reportInfo("Cleanup called");
}

int main(int argc, char *argv[])
{
    char buffer[256];
    if (argc < 2)
    {
        fprintf(stderr, "ERROR, no port provided\n");
        exit(1);
    }

    project_init(&projects);
    signal(SIGPIPE, SIG_IGN);

    atexit(cleanup);

    int sockfd = createServerSocket(atoi(argv[1]));
    int status = 0;
    while (1 == 1)
    {
        listen(sockfd, 5);
        struct sockaddr_in cli_addr;
        socklen_t clilen = sizeof(cli_addr);
        int newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
        if (newsockfd < 0)
        {
            reportError("ERROR on accept");
            close(sockfd);
            return -1;
        }
        else
        {
            printf("\n");
            reportInfo("Accept connection from client file descriptor=%d", newsockfd);
        }
        pthread_t thread;
        pthread_create(&thread, 0, processConnection, (void *)&newsockfd);
        pthread_detach(thread);
    }
    close(sockfd);
    return status;
}
