#ifndef __SOCK_UTILS_HEADER__
#define __SOCK_UTILS_HEADER__

#include <errno.h>
#include <netdb.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/socket.h>

extern bool isClient();

int createNConnectClientSocket(char *hostname, int port)
{
    struct sockaddr_in serv_addr;
    struct hostent *server;

    char buffer[256];
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        reportError("ERROR opening socket");
        return sockfd;
    }
    server = gethostbyname(hostname);
    if (server == NULL)
    {
        fprintf(stderr, "ERROR, no such host\n");
        close(sockfd);
        return -1;
    }
    bzero((char *)&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr,
          (char *)&serv_addr.sin_addr.s_addr,
          server->h_length);
    serv_addr.sin_port = htons(port);
    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        reportError("ERROR connecting");
        close(sockfd);
        return -1;
    }
    reportInfo("Connect to hostname/IP:%s port:%hd", hostname, port);
    return sockfd;
}

int sendBytes(int sockfd, const char *buf, int len)
{
    int total = 0;
    int numRetries = 5;
    while (total < len)
    {
        int bytesSent;
        int i = 0;
        for (; i < numRetries; i++)
        {
            bytesSent = write(sockfd, buf + total, len - total);
            if (bytesSent < 0 && errno != EAGAIN && errno != EINTR && errno != EWOULDBLOCK)
            {
                return bytesSent;
            }
            if (bytesSent >= 0)
            {
                break;
            }
            sleep(1);
        }
        total += bytesSent;
    }
    //printf("TOTAL:%d\n",total);
    return total;
}

int recvBytes(int sockfd, char *buf, int len)
{
    int total = 0;
    int numRetries = 5;
    while (total < len)
    {
        int bytesRecv;
        int i = 0;
        for (; i < numRetries; i++)
        {
            bytesRecv = read(sockfd, buf + total, len - total);
            if (bytesRecv == 0)
            {
                //printf("Remote closed connection read %d bytes errno=%d =%s\n",bytesRecv, errno,strerror(errno));
                if (isClient())
                {
                    reportError("Lost connection to server");
                }
                else
                {
                    reportError("Lost connection to client");
                }
                return -1;
            }
            if (bytesRecv < 0 && errno != EAGAIN && errno != EINTR && errno != EWOULDBLOCK)
            {
                return bytesRecv;
            }
            if (bytesRecv > 0)
            {
                break;
            }
            sleep(1);
        }
        total += bytesRecv;
    }
    //printf("Total:%d\n",total);
    return total;
}

//Shall only be FAILED, SUCCEED
int sendStatusToClient(int sockfd, const char *status)
{
    return sendBytes(sockfd, status, strlen(status));
}

#endif
