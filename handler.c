#include "handler.h"

const char* HTTP_METHOD_STRING[] = {
    "GET", "POST", "PUT", "DELETE", "CONNECT",
    "PATCH", "OPTIONS", "TRACE", "HEAD"
};
const char* HTTP_VERSION_STRING[] = {
    "1.0", "1.1", "2.0"
};
const struct http_status_object HTTP_STATUS_ALL[] = {
    { 200, "OK" },
    { 400, "Bad Request"},
    { 403, "Forbidden" },
    { 404, "Not Found" },
    { 500, "Internal Server Error" },
    { 501, "Not Implemented" },
    { 505, "HTTP Version Not Supported" }
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
        n = send(sfd, data+total, bytesleft, 0);
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
fill_http_req(struct HTTP_REQ* http_req, const char* method, char* uri, char v1, char v2)
{
    int i;
    int end;

    for(i = 0, end = HEAD - GET; i <= end; ++i)
    {
        if(0 == strcmp(method, HTTP_METHOD_STRING[i]))
        {
            http_req->method = i;
            break;
        }
        else if(i == end)
        {
            return -1;
        }
    }

    char version[] = {v1, '.', v2, '\0'};
    for(i = 0, end = V20 - V10; i <= end; ++i)
    {
        if(0 == strcmp(version, HTTP_VERSION_STRING[i]))
        {
            http_req->version = i;
            break;
        }
        else if(i == end)
        {
            http_req->status = HTTP_VER;
            return -1;
        }
    }

    http_req->uri = uri;
    http_req->status = OK;

    return 0;
}

void
parse_http_req(struct HTTP_REQ* http_req, const char* req)
{
    char method[8];
    char* uri = NULL;
    char v1 = 0;
    char v2 = 0;

    http_req->status = BAD_REQUEST;

    const char* fstr = "%7[A-Z] %m[-A-Za-z0-9./_] HTTP/%c.%c";
    int n = sscanf(req, fstr, &method, &uri, &v1, &v2);

    if(4 == n)
    {
        if(0 == fill_http_req(http_req, method, uri, v1, v2))
        {
            print_http_req(http_req);
        }
    }
    else if(0 < n)
    {
        fprintf(stderr, "Not all values were matched\n");
        if(NULL != uri)
        {
            free(uri);
        }
    }
    else if(0 == n)
    {
        fprintf(stderr, "No matching values\n");
    }
    else
    {
        perror("scanf()");
        http_req->status = INTERNAL_ERROR;
    }
}

void
do_http_get(int sfd, struct HTTP_REQ* http_req)
{
    const size_t rsize = 512;
    size_t ssize;
    char buf[rsize];
    int n;
    int fd;

    char* path = strcmp(http_req->uri, "/") ? http_req->uri : "/index.html";
    if(-1 == (fd = open(path, O_RDONLY)))
    {
        http_req->status = (EACCES == errno) ? FORBIDDDEN : NOT_FOUND;
        return;
    }

    while(0 != (n = read(fd, buf, rsize)))
    {
        if(n != -1)
        {
            ssize = n;
            if(-1 == sendall(sfd, buf, &ssize))
            {
                fprintf(stderr, "%d: exp %d, sent %ld\n", sfd, n, rsize);
            }
        }
        else
        {
            perror("read()"); // Internal Server Error
            return;
        }
    }
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
    char resp[400];
    sprintf(resp, "HTTP/%s %d %s\r\nContent-Type: text/html\r\n\r\n<H1>%d: %s</H1>",
            HTTP_VERSION_STRING[http_req->version],
            HTTP_STATUS_ALL[http_req->status].st,
            HTTP_STATUS_ALL[http_req->status].str,
            HTTP_STATUS_ALL[http_req->status].st,
            HTTP_STATUS_ALL[http_req->status].str);
    size_t size = strlen(resp);
    sendall(sfd, resp, &size);
    switch(http_req->status)
    {
        case OK:
            break;
        case BAD_REQUEST:
            break;
        case NOT_FOUND:

            break;
        case INTERNAL_ERROR:
            break;
        case HTTP_VER:
            fprintf(stderr, "%s\n", HTTP_STATUS_ALL[HTTP_VER].str);
            break;
        default:
            break;
    }
}

void
make_response(int sfd, const char* data)
{
    struct HTTP_REQ http_req;
    parse_http_req(&http_req, data);
    
    if(OK == http_req.status)
    {
        do_http_req(sfd, &http_req);
        free(http_req.uri);
    }
    else
    {
        error_http(sfd, &http_req);
    }
}
