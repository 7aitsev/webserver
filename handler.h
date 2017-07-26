#ifndef HANDLER_H
#define HANDLER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

void
print_http_req(const struct HTTP_REQ* http_req);
int
fill_http_req(struct HTTP_REQ* http_req,
              const char* method, char* uri, char v1, char v2);
void
parse_http_req(struct HTTP_REQ* http_req, const char* req);

void
do_http_get(const struct HTTP_REQ* http_req, char* presp, size_t* psize);

void
do_http_req(struct HTTP_REQ* http_req, char* presp, size_t* psize);

void
error_http(const struct HTTP_REQ* http_req, char* presp, size_t* psize);

void
make_response(const char* data, char* presp, size_t* psize);

#endif
