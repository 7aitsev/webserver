#ifndef HANDLER_H
#define HANDLER_H

#include <stddef.h>

#define DEF_BUF_SIZE 512
#define URI_SIZE 256

enum HTTP_METHOD {
    GET, POST, PUT, DELETE, CONNECT, PATCH, OPTIONS, TRACE, HEAD
};

enum HTTP_STATUS {
    OK = 0,
    BAD_REQUEST = 2,
    FORBIDDDEN = 4,
    NOT_FOUND = 6,
    INTERNAL_ERROR = 8,
    NOT_IMPLEMENTED = 10,
    HTTP_VER = 12
};

enum HTTP_VERSION { V10, V11 };

struct HTTP_REQ
{
    enum HTTP_METHOD method;
    char uri[URI_SIZE];
    enum HTTP_VERSION version;
    enum HTTP_STATUS status;
};

// network i/o
int sendall(int sfd, const char* data, size_t* dsize);

int isstrin(const char* str, const char * const set[], size_t latest_el);

size_t
content_type(const char *path);

size_t
put_http_header(char* buf, const struct HTTP_REQ* http_req,
                const char* path);

void
print_http_req(const struct HTTP_REQ* http_req);

int
fill_http_req(struct HTTP_REQ* http_req,
              const char* method, char* uri, char v1, char v2);
int
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
