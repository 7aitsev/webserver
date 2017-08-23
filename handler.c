#include "config.h"
#include "handler.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>

const char * const HTTP_METHOD_STRING[] = {
    "GET", "POST", "PUT", "DELETE", "CONNECT",
    "PATCH", "OPTIONS", "TRACE", "HEAD"
};

const char * const HTTP_VERSION_STRING[] = {
    "1.0", "1.1"
};

const char * const HTTP_STATUS_ALL[] = {
    "200", "OK",
    "400", "Bad Request",
    "403", "Forbidden",
    "404", "Not Found",
    "500", "Internal Server Error",
    "501", "Not Implemented",
    "505", "HTTP Version Not Supported"
};

const char * const mime[] = {
    ".html", "text/html",
    ".htm", "text/html",
    ".css", "text/css",
    ".gif", "image/gif",
    ".png", "image/png",
    ".jpg", "image/jpeg",
    ".xml", "application/xml",
    ".svg", "image/svg+xml",
    ".txt", "text/plain",
    "application/octet-stream"
};

void
print_http_req(const struct HTTP_REQ* http_req)
{
    printf("%s %s HTTP/%s\n",
            HTTP_METHOD_STRING[http_req->method],
            http_req->uri,
            HTTP_VERSION_STRING[http_req->version]
          );
}

int sendall(int sfd, const char* data, size_t* dsize)
{
    int total = 0;
    int bytesleft = *dsize;
    int n;

    while(total < *dsize)
    {
        n = send(sfd, data+total, bytesleft, MSG_NOSIGNAL);
        if(-1 == n)
        {
            break;
        }
        total += n;
        bytesleft -= n;
    }

    *dsize = total;

    return n == -1 ? -1 : 0;
}

int
isstrin(const char* str, const char * const set[], size_t latest_el)
{
    size_t i;

    for(i = 0; ; ++i)
    {
        if(0 == strcmp(str, set[i]))
        {
            return i;
        }
        else if(i == latest_el)
        {
            return -1;
        }
    }
}

size_t
content_type(const char *path)
{
    size_t i;

    for(i = 0; i < sizeof(mime) / sizeof(*mime); i += 2)
    {
        if(strstr(path, mime[i]))
        {
            return (size_t) i + 1;
        }
    }
    return (size_t) i - 2;
}

size_t
put_http_header(char* buf, const struct HTTP_REQ* http_req,
                const char* path)
{
    const char* mimetype = (path) ? mime[content_type(path)] : mime[1];

    sprintf(buf, "HTTP/%s %s %s\r\nContent-type: %s\r\n\r\n",
            HTTP_VERSION_STRING[http_req->version],
            HTTP_STATUS_ALL[http_req->status],
            HTTP_STATUS_ALL[http_req->status + 1],
            mimetype);

    return strlen(buf);
}

int
fill_http_req(struct HTTP_REQ* http_req, const char* method,
        char* uri, char v1, char v2)
{
    enum HTTP_METHOD http_method;
    enum HTTP_VERSION http_version;

    if(-1 == (http_method = isstrin(method, HTTP_METHOD_STRING, HEAD)))
    {
        http_req->status = BAD_REQUEST;
        return -1;
    }

    const char version[] = {v1, '.', v2, '\0'};
    if(-1 == (http_version = isstrin(version, HTTP_VERSION_STRING, V11)))
    {
        http_req->status = HTTP_VER;
        return -1;
    }

    http_req->method = http_method;
    http_req->version = http_version;

    strncpy(http_req->uri, uri, URI_SIZE);
    if(http_req->uri[URI_SIZE - 1] != '\0')
    {
        // path is too long, but the request itself is okay
        http_req->status = INTERNAL_ERROR;
        //http_req->uri[URI_SIZE - 1] = '\0';
        return -1;
    }
    free(uri);

    return 0;
}

int
parse_http_req(struct HTTP_REQ* http_req, const char* req)
{
    char method[9];
    char* uri = NULL;
    char v1 = 0;
    char v2 = 0;

    int n;
    errno = 0;
    const char* fstr = "%8[A-Z] %m[-A-Za-z0-9./_] HTTP/%c.%c";
    if(4 == (n = sscanf(req, fstr, &method, &uri, &v1, &v2)))
    {
        return fill_http_req(http_req, method, uri, v1, v2);
    }
    else if(0 < n)
    {
        fprintf(stderr, "Not all values were matched\n");
        if(NULL != uri)
        {
            free(uri);
        }
        http_req->status = BAD_REQUEST;
    }
    else if(0 == n && 0 == errno)
    {
        fprintf(stderr, "No matching values\n");
        http_req->status = BAD_REQUEST;
    }
    else
    {
        perror("scanf()");
        http_req->status = INTERNAL_ERROR;
    }
    
    return -1;
}

void
do_http_get(int sfd, struct HTTP_REQ* http_req)
{
    char buf[DEF_BUF_SIZE];
    size_t ssize;
    int fd;
    int n;

    char* path = strcmp(http_req->uri, "/")
            ? http_req->uri
            : CONFIG.index_page;
printf("path = %s\n", path);
    fd = open(path, O_RDONLY);
    if(-1 != fd)
    {
        http_req->status = OK;
    }
    else
    {
        switch(errno)
        {
            case EACCES:
                http_req->status = FORBIDDDEN;
                return;
            default: // ENOENT
                http_req->status = NOT_FOUND;
                return;
        }
    }
    
    size_t h = put_http_header(buf, http_req, path);

    while(0 != (n = read(fd, buf + h, DEF_BUF_SIZE - h)))
    {
        if(n != -1)
        {
            ssize = n + h;
            if(-1 != sendall(sfd, buf, &ssize))
            {
                h = 0;
            }
            else
            {
                fprintf(stderr, "%d: exp %ld, sent %ld\n", sfd, n + h, ssize);
                break;
            }
        }
        else
        {
            perror("read()"); // Internal Server Error
            break;
        }
    }

    close(fd);
}

void
do_http_req(int sfd, struct HTTP_REQ* http_req)
{
    switch(http_req->method)
    {
        case GET:
            do_http_get(sfd, http_req);
            break;
        default:
            http_req->status = NOT_IMPLEMENTED;
    }

    if(OK != http_req->status)
    {
        error_http(sfd, http_req);
    }
}

void
error_http(int sfd, const struct HTTP_REQ* http_req)
{
    char resp[300];
    size_t size;

    size = put_http_header(resp, http_req, NULL);

    sprintf(resp + size,
            "<html><title>%s %s</title><body><html><h2>%s: %s</h2></html>",
            HTTP_STATUS_ALL[http_req->status],
            HTTP_STATUS_ALL[http_req->status + 1],
            HTTP_STATUS_ALL[http_req->status],
            HTTP_STATUS_ALL[http_req->status + 1]);
    size = strlen(resp);
    sendall(sfd, resp, &size);
    fprintf(stderr, "%s\n", resp);
}

void
make_response(int sfd, const char* data)
{
    struct HTTP_REQ http_req;

    if(0 == parse_http_req(&http_req, data))
    {
        do_http_req(sfd, &http_req);
    }
    else
    {
        error_http(sfd, &http_req);
    }
}
