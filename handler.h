#ifndef HANDLER_H
#define HANDLER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>

#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include <unistd.h>

enum HTTP_METHOD {
    GET, POST, PUT, DELETE, CONNECT, PATCH, OPTIONS, TRACE, HEAD
};

enum HTTP_STATUS {
    OK,
    BAD_REQUEST,
    FORBIDDDEN,
    NOT_FOUND,
    INTERNAL_ERROR,
    NOT_IMPLEMENTED,
    HTTP_VER
};

struct http_status_object {
    unsigned short int st;
    char str[30];
};

enum HTTP_VERSION { V10, V11, V20 };

struct HTTP_REQ
{
    enum HTTP_METHOD method;
    char* uri;
    enum HTTP_VERSION version;
    enum HTTP_STATUS status;
};

struct HTTP_RESP
{
    enum HTTP_VERSION version;
    enum HTTP_STATUS status;
    char* payload;
    size_t pl_siz;
};

// network i/o
int sendall(int sfd, const char* data, size_t* dsize);


void
print_http_req(const struct HTTP_REQ* http_req);
int
fill_http_req(struct HTTP_REQ* http_req,
              const char* method, char* uri, char v1, char v2);
void
parse_http_req(struct HTTP_REQ* http_req, const char* req);

void
do_http_get(int sfd, struct HTTP_REQ* http_req);

void
do_http_req(int sfd, struct HTTP_REQ* http_req);

void
error_http(int sfd, const struct HTTP_REQ* http_req);

void
make_response(int sfd, const char* data);

#endif
