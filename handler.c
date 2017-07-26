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
do_http_get(const struct HTTP_REQ* http_req, char* presp, size_t* psize)
{
    FILE* file = fopen(http_req->uri, "r");


}

void
do_http_req(struct HTTP_REQ* http_req, char* presp, size_t* psize)
{
    switch(http_req->method)
    {
        case GET:
            do_http_get(http_req, presp, psize);
            break;
        default:
            http_req->status = NOT_IMPLEMENTED;
    }
}

void
error_http(const struct HTTP_REQ* http_req, char* presp, size_t* psize)
{
    switch(http_req->status)
    {
        case OK:
            break;
        case BAD_REQUEST:
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
make_response(const char* data, char* presp, size_t* psize)
{
    struct HTTP_REQ http_req;
    parse_http_req(&http_req, data);
    
    if(OK == http_req.status)
    {
        do_http_req(&http_req, presp, psize);
        free(http_req.uri);
    }
    else
    {
        error_http(&http_req, presp, psize);
    }
}
