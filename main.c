#include <string.h> // for memset
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>

#define BACKLOG_SIZE 3

#include "handler.h"

int main(int argc, char** argv)
{
    int status;
    struct addrinfo hints;
    struct addrinfo* servinfo;
    struct addrinfo* p;
    int sfd;
    
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    hints.ai_protocol = IPPROTO_TCP;

    if(0 != (status = getaddrinfo(NULL, "http", &hints, &servinfo)))
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
        exit(EXIT_FAILURE);
    }

    int yes = 1;
    for(p = servinfo; NULL != p; p = p->ai_next)
    {
        if(-1 == (sfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)))
        {
            continue;
        }

        if(-1 == setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes))) {
            perror("setsockopt");
            exit(EXIT_FAILURE);
        }

        if(0 == bind(sfd, p->ai_addr, p->ai_addrlen))
        {
            break;
        }

        close(sfd);
    }

    if(p == NULL)
    {
        fprintf(stderr, "Could not bind\n");
        exit(EXIT_FAILURE);
    }

    freeaddrinfo(servinfo);

    if(-1 == listen(sfd, BACKLOG_SIZE))
    {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    int i;
    int fd;
    int fdmax = sfd;
    fd_set cache;
    fd_set work;
    struct sockaddr_storage client;
    socklen_t addr_size = sizeof(client);
    int cs;
    int bytes;
    char buf[1024];

    FD_ZERO(&cache);
    FD_SET(sfd, &cache);
    
    prepareRegex();
    while(1)
    {
        work = cache;
        status = select(fdmax + 1, &work, NULL, NULL, NULL);
        if(status)
        {
            for(i = 3; i <= fdmax; ++i)
            {
                if(FD_ISSET(i, &work))
                {
                    if(i != sfd)
                    {
                        if(0 < (bytes = recv(i, (void*) buf, 512, 0)))
                        {
                            parseReq(buf, bytes);
                            printf("from %d:\n\t%s\n", i, buf);
                            const char* msg = "HTTP/1.0 200 OK\nContent-type: text/html\n\n<H1>Success!</H1>";
                            send(i, (void *) msg, 57, 0);
                            close(i);
                            FD_CLR(i, &cache);
                        }
                        else
                        {
                            if(bytes == 0)
                            {
                                printf("socket %d hung up\n", i);
                            }
                            else
                            {
                                perror("recv()");
                            }
                            close(i);
                            FD_CLR(i, &cache);
                        }
                    }
                    else
                    {
                        addr_size = sizeof(client);
                        if(-1 != (cs = accept(sfd, (struct sockaddr*) &client, &addr_size)))
                        {
                            FD_SET(cs, &cache);
                            fdmax = (cs > fdmax) ? cs : fdmax;
                            printf("new client: %d\n", cs);
                        }
                        else
                        {
                            perror("accept()");
                        }
                    }
                }
            }
        }
        else
        {
            perror("select()");
            break;
        }
    }
    finishRegex();

    return EXIT_SUCCESS;
}
