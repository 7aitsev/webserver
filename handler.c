#include "handler.h"

void errorRegex(int errcode, const char* msg)
{
    size_t eb_size = 1024;
    char errbuf[eb_size];
    regerror(errcode, &reg_http_get, errbuf, eb_size);
    fprintf(stderr, msg, errbuf);
}

void prepareRegex()
{
    const char* regex = "^GET .{1,256} HTTP\\/1\\..$";
    int flags = REG_EXTENDED | REG_ICASE | REG_NOSUB;
    int retval;
    if(0 != (retval = regcomp(&reg_http_get, regex, flags)))
    {
        errorRegex(retval, "regcomp():\n%s\n");
        exit(EXIT_FAILURE);
    }
}

void finishRegex()
{
    regfree(&reg_http_get);
}

int parseReq(const char * const req, size_t req_size)
{
    // copy *single* line
    size_t limit = (270 < ++req_size) ? 270 : req_size;
    char line[limit];
    size_t cnt = 0;
    char ch = req[0];
    while('\n' != ch && '\0' != ch && cnt < limit - 1)
    {
        line[cnt] = req[cnt];
        ++cnt;
        ch = req[cnt];
    }
    line[cnt] = '\0';

    int retval;
    if(0 == (retval = regexec(&reg_http_get, line, 0, NULL, 0)))
    {
        printf("HTTP request: OK\n\t%s\n", line);
    }
    else if(REG_NOMATCH == retval)
    {
        fprintf(stderr, "HTTP request is invalid\n");
    }
    else
    {
        errorRegex(retval, "regexec();\n%s\n");
    }
    return retval;
}
