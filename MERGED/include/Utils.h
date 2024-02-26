#ifndef __UTILS_HEADER__
#define __UTILS_HEADER__

#include "Digest.h"
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

extern const char* getTaskName();

void reportError(const char *fmt, ...)
{
    va_list args;
    char buf[1024];
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    fprintf(stderr, "%s:%s\n", getTaskName(), buf);
    //printf("%s\n",buf);
}

void reportInfo(const char *fmt, ...)
{
    va_list args;
    char buf[1024];
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    printf("%s:%s\n", getTaskName(), buf);
}

int fileExist(const char *filename)
{
    if (access(filename, F_OK) < 0)
    {
        return -1;
    }
    else
    {
        return 0;
    }
}

int compare2Files(const char *filename1, const char *filename2)
{
    struct stat st1, st2;
    if (stat(filename1, &st1) < 0 || stat(filename2, &st2) < 0)
    {
        return -1;
    }
    if (st1.st_size != st2.st_size)
    {
        return -1;
    }
    char *buf1 = (char *)malloc(st1.st_size);
    char *buf2 = (char *)malloc(st2.st_size);
    int fd1 = open(filename1, O_RDONLY);
    int fd2 = open(filename2, O_RDONLY);
    read(fd1, buf1, st1.st_size);
    read(fd2, buf2, st2.st_size);
    close(fd1);
    close(fd2);
    if (memcmp(buf1, buf2, st1.st_size) == 0)
    {
        free(buf1);
        free(buf2);
        return 0;
    }
    else
    {
        free(buf1);
        free(buf2);
        return -1;
    }
}

char *readFile(const char *filename, size_t sz)
{
    int fd = open(filename, O_RDONLY);
    if (fd < 0)
    {
        reportError("Failed to open file:%s %d=%s", filename, errno, strerror(errno));
        return NULL;
    }

    char *buf = (char *)malloc(sz);
    int nr = read(fd, buf, sz);
    if (nr < sz)
    {
        reportError("Failed to read %d bytes from file %s", sz, filename);
        free(buf);
        close(fd);
        return NULL;
    }
    close(fd);
    return buf;
}

int calcDigest(char hashstr[], char *filename)
{
    struct stat st;
    if (stat(filename, &st) < 0)
    {
        return -1;
    }
    char *buf = readFile(filename, st.st_size);
    if (buf != NULL)
    {
        uint8_t hash[32];
        calc_sha_256(hash, buf, st.st_size);
        //printf("SIZE=%d buf=%s\n",st.st_size, buf);
        int i = 0;
        for (; i < 32; i++)
        {
            sprintf(&hashstr[2 * i], "%02x", hash[i]);
        }
        hashstr[64] = '\0';
        //printf("CALC:HASH=%s\n",hashstr);
        return 0;
    }
    return -1;
}

void fullFilename(char fullname[], const char *project, const char *filename)
{
    int plen = strlen(project), flen = strlen(filename);
    int total = plen + flen + 2;
    snprintf(fullname, total, "%s/%s", project, filename);
}

int openSpecialFile(FILE **pfile, const char *project, const char *filename)
{
    if (*pfile != NULL)
    {
        return 0;
    }

    char fullname[128];
    fullFilename(fullname, project, filename);
    *pfile = fopen(fullname, "w");
    if (*pfile == NULL)
    {
        reportError("Failed to open file %s for write", fullname);
        return -1;
    }
    return 0;
}

int displayHistory(const char *project)
{
    FILE *history = fopen("history", "r");
    if (history == NULL)
    {
        reportError("File history does not exist in current directory");
        return -1;
    }
    char line[128];
    while (fgets(line, sizeof(line), history))
    {
        printf("%s", line);
    }
    return 0;
}

#endif
