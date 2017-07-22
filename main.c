#include <string.h> // for memset
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>

#define BACKLOG_SIZE 3

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

    struct sockaddr_storage client;
    socklen_t addr_size = sizeof(client);
    int cs;
    if(-1 == (cs = accept(sfd, (struct sockaddr*) &client, &addr_size)))
    {
        perror("accept");
        exit(EXIT_FAILURE);
    }

    const char* msg =
        "HTTP/1.0 200 OK\nContent-type: text/html\n\n<H1>Success!</H1>";
    send(cs, (void *) msg, 57, 0);

    close(cs);
    close(sfd);

    return EXIT_SUCCESS;
}
