#ifndef __PACK_HEADER__
#define __PACK_HEADER__

#include <fcntl.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <unistd.h>

#include "MessageType.h"
#include "SockUtils.h"
#include "Utils.h"
#include "VectorStr.h"

int encodeAndSendCommand(int sockfd, short cmd, char project[])
{
    char buf[1024];
    memset(buf, '\0', sizeof(buf));
    int offset = 0;
    cmd = htons(cmd);
    memcpy(&buf[offset], (char *)&cmd, sizeof(short));
    short len = strlen(project);
    offset += sizeof(short);
    len = htons(len);
    memcpy(&buf[offset], (char *)&len, sizeof(short));
    offset += sizeof(short);
    memcpy(&buf[offset], project, strlen(project));
    offset += strlen(project);
    if (sendBytes(sockfd, buf, offset) < offset)
    {
        //shall use the name for command
        reportError("Failed to send command %d for project %s", cmd, project);
        return -1;
    }
    return 0;
}

int readAndDecodeCommand(int sockfd, short *cmd, char project[])
{
    short cmdNLen[2];
    if (recvBytes(sockfd, (char *)&cmdNLen, sizeof(cmdNLen)) < (int)sizeof(cmdNLen))
    {
        reportError("Failed to read command and length of project from socket");
        close(sockfd);
        return -1;
    }

    *cmd = cmdNLen[0];
    *cmd = ntohs(*cmd);
    short len = cmdNLen[1];
    len = ntohs(len);
    if (recvBytes(sockfd, project, len) < len)
    {
        reportError("Failed to read project name of length %d from socket", len);
        close(sockfd);
        return -1;
    }

    project[len] = '\0';
    return 0;
}

int encodeAndSendFileHeader(int sockfd, const char *filename, size_t *size)
{
    struct stat st;
    if (stat(filename, &st) < 0)
    {
        reportError("Failed to get file status of %s", filename);
        return -1;
    }
    *size = st.st_size;
    char buf[1024];
    long total = strlen(filename) + 1 + (*size);
    long t = total;
    total = htonl(total);
    memcpy(&buf, &total, sizeof(long));
    memcpy(&buf[sizeof(long)], filename, strlen(filename));
    buf[sizeof(long) + strlen(filename)] = '\0';
    int bsize = sizeof(long) + strlen(filename) + 1;
    if (sendBytes(sockfd, buf, bsize) < bsize)
    {
        reportError("Failed to send header for file %s:", filename);
        return -1;
    }
    return 0;
}

int encodeAndSendFileContent(int sockfd, const char *filename, size_t size)
{
    char *buf = readFile(filename, size);
    if (buf == NULL)
    {
        return -1;
    }
    if (sendBytes(sockfd, buf, size) < size)
    {
        free(buf);
        return -1;
    }
    free(buf);
    return 0;
}

int writeFile(const char *filename, char data[], size_t nbytes)
{
    char cmdbuf[1024];
    snprintf(cmdbuf, sizeof(cmdbuf), "mkdir -p $(dirname %s)", filename);
    system(cmdbuf);

    int fd = open(filename, O_CREAT | O_WRONLY | O_TRUNC, 0777);
    if (fd < 0)
    {
        reportError("Failed to open file %s for write %d=%s", filename, errno, strerror(errno));
        return -1;
    }

    if (write(fd, data, nbytes) < nbytes)
    {
        close(fd);
        reportError("Failed to write data to file %s %d=%s", filename, errno, strerror(errno));
        return -1;
    }

    close(fd);
    return 0;
}

int encodeAndSendFile(int sockfd, const char *filename)
{
    reportInfo("encodeAndSendFile FILE:%s", filename);
    size_t size;
    if (encodeAndSendFileHeader(sockfd, filename, &size) < 0)
    {
        close(sockfd);
        return -1;
    }

    if (encodeAndSendFileContent(sockfd, filename, size) < 0)
    {
        close(sockfd);
        return -1;
    }

    return 0;
}

int decodeAndSaveFile(int sockfd, const char *provideName)
{
    long total;
    if (recvBytes(sockfd, (char *)&total, sizeof(long)) < (int)sizeof(long))
    {
        reportError("Failed to read file length from socket");
        close(sockfd);
        return -1;
    }
    total = ntohl(total);
    char *buf = (char *)malloc(total);
    if (buf == NULL)
    {
        reportError("Failed to allocate memory of size %lu bytes", total);
        return -1;
    }
    char *obuf = buf;
    if (recvBytes(sockfd, buf, total) < total)
    {
        reportError("Failed to read %d bytes from socket", total);
        free(buf);
        close(sockfd);
        return -1;
    }
    char filename[1024];
    memset(filename, '\0', sizeof(filename));
    strcpy(filename, buf);
    int nlen = strlen(filename);
    buf += nlen + 1;
    if (strcmp(provideName, "") != 0)
    {
        memset(filename, '\0', sizeof(filename));
        strcpy(filename, provideName);
    }
    if (writeFile(filename, buf, total - nlen - 1) < 0)
    {
        reportError("Failed to write file name=%s size=%lu", filename, total - nlen - 1);
        free(obuf);
        return -1;
    }
    free(obuf);
    return 0;
}

int encodeAndSendFilesCompressed(int sockfd, VectorStr *vec)
{
    char *cmd = (char *)malloc(str_size(vec) * 128);
    int randInt = rand();
    char filename[128];
    snprintf(filename, sizeof(filename), "%d.tar.gz", randInt);
    snprintf(cmd, str_size(vec) * 128, "tar czf %s", filename);
    reportInfo("Compress:%s", cmd);
    int idx = 0;
    for (; idx < str_size(vec); idx++)
    {
        strcat(cmd, " ");
        strcat(cmd, str_getData(vec, idx));
    }
    if (system(cmd) < 0)
    {
        reportError("Failed to execute cmd:%s", cmd);
        free(cmd);
        return -1;
    }
    encodeAndSendFile(sockfd, filename);
    free(cmd);
    unlink(filename);
    return 0;
}

int uncompressFile(const char *filename)
{
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "tar xzf %s", filename);
    return system(cmd);
}

int encodeAndSendFiles(int sockfd, VectorStr *vec)
{
    return encodeAndSendFilesCompressed(sockfd, vec);
    short nsz = str_size(vec);
    short sz = nsz;
    reportInfo("There are %d files", sz);
    //encode and send a short int
    nsz = htons(nsz);
    if (sendBytes(sockfd, (char *)&nsz, sizeof(nsz)) < sizeof(nsz))
    {
        close(sockfd);
        return -1;
    }

    int i = 0;
    for (; i < sz; i++)
    {
        //printf("Processing I:%d file:%s\n", i, vec[i].c_str());
        if (encodeAndSendFile(sockfd, str_getData(vec, i)) < 0)
        {
            return -1;
        }
    }

    return 0;
}

int encodeAndSendDirectory(int sockfd, const char *dirname)
{
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "find %s -type f -exec ls -1 {} \\;", dirname);
    FILE *fp = popen(cmd, "r");
    if (fp == NULL)
    {
        reportError("Failed to get the output of cmd:%s", cmd);
        close(sockfd);
        return -1;
    }

    VectorStr vec;
    str_init(&vec);
    char buf[1024];
    memset(buf, '\0', sizeof(buf));
    while (fgets(buf, sizeof(buf), fp))
    {
        char *p = strchr(buf, '\n');
        if (p)
        {
            *p = '\0';
        }
        str_append(&vec, buf);
        memset(buf, '\0', sizeof(buf));
    }
    //return encodeAndSendFiles(sockfd,&vec);
    return encodeAndSendFilesCompressed(sockfd, &vec);
}

int decodeAndSaveFilesCompressed(int sockfd)
{
    int randInt = rand();
    char filename[128];
    snprintf(filename, sizeof(filename), "%d.tar.gz", randInt);
    decodeAndSaveFile(sockfd, filename);
    char command[128];
    snprintf(command, sizeof(command), "tar xzf %s", filename);
    reportInfo("Uncompress %s", command);
    system(command);
    unlink(filename);
    return 0;
}

int decodeAndSaveFiles(int sockfd)
{
    return decodeAndSaveFilesCompressed(sockfd);
    short sz;
    if (recvBytes(sockfd, (char *)&sz, sizeof(sz)) < (int)sizeof(sz))
    {
        close(sockfd);
        return -1;
    }

    sz = ntohs(sz);

    reportInfo("SZ:%d", sz);
    int i = 0;
    for (; i < sz; i++)
    {
        if (decodeAndSaveFile(sockfd, "") < 0)
        {
            close(sockfd);
            return -1;
        }
    }

    return 0;
}

int decodeAndSaveDirectory(int sockfd)
{
    return decodeAndSaveFiles(sockfd);
}

#endif
