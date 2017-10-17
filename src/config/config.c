#include "config/config.h"
#include "config/conf.h"

#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct conf g_conf;
#define WEB_SERVER_VERSION "1.0"
#define DEF_INDEX_PAGE "/index.html" 
#define DEF_LOG_PATH "/tmp/webserver.log"
#define DEF_NO_LOG "/dev/null"
#define DEF_PORT "http"
#define DEF_HOST NULL
#define DEF_USER_NAME "root"
#define DEF_GROUP_NAME "root"

#define OPTSTRING "vhlr:i:u:g:"
static const struct option g_lopts[] = {
    {"document-root", required_argument, NULL, 'r'},
    {"index-page", required_argument, NULL, 'i'},
    {"log", required_argument, NULL, 1},
    {"port", required_argument, NULL, 2},
    {"host", required_argument, NULL, 3},
    {"user", required_argument, NULL, 'u'},
    {"group", required_argument, NULL, 'g'}, 
    {"help", no_argument, NULL, 'h'},
    {"version", no_argument, NULL, 'v'},
    {NULL     , 0          , NULL, 0}
};
static const char * const g_params
    = "document-root index-page log port host user group";

static void
printhelp()
{
    printf(
"WebServer %s\n\n"
"Usage: webserver [options] -r root directory\n"
"         - start the server in a specified root directory.\n"
"       webserver [options] config_file\n"
"         - start the server and load config options from a file\n\n"
"Options:\n"
"-r, --document-root root    : Set a path to a web site directory\n"
"-i, --index-page index      : Set a home page path\n"
"-l                          : Do not use a log file\n"
"--log log_path              : Set a path for a log file\n"
"--port port                 : A port on which the server will listen\n"
"--host host                 : Listen for the given IP adress\n"
"-u, --user user             : Change the user id of the process\n"
"-g, --group group           : Change the group id of the process\n"
"-h, --help                  : Print help (this message) and exit\n"
"-v, --version               : Print version information and exit\n",
            WEB_SERVER_VERSION
        );
}

static int
allocopts()
{
    int nargs = 2 * sizeof(g_lopts) / sizeof(struct option);
    int arglen = 256;
    g_conf.opts = malloc(nargs * sizeof(char*) + nargs * arglen);
    if(NULL != g_conf.opts)
    {
        int i = 0;
        char *offset = (char*) &g_conf.opts[nargs];
        for(; i < nargs; ++i, offset += arglen)
        {
            g_conf.opts[i] = offset;
        }
        strcpy(g_conf.opts[0], "./webserver");
        return 0;
    }
    else
    {
        perror("[config] malloc for g_conf.opts failed");
        return -1;
    }
}

static int
isstrblank(const char* s)
{
    while(*s != '\0')
    {
        if(!isspace(*s))
            return 0;
        ++s;
    }
    return 1;
}

static int
readargs(const char* fline, int * const nopt)
{
    int rv;
    char param[16]; // 15+1
    char arg[256]; // 255+1

    if(1 == isstrblank(fline))
        return 0;

    if(2 == (rv = sscanf(fline, "%15[-a-z] %255s", param, arg)))
    {
        if(NULL != strstr(g_params, param))
        {
            sprintf(g_conf.opts[++(*nopt)], "--%s", param);
            strcpy(g_conf.opts[++(*nopt)], arg);
        }
        else
        {
            fprintf(stderr, "[config] Parameter \"%s\" in the config"
                    "is invalid\n", param);
            return -1;
        }
    }
    else if(0 <= rv && 0 == errno)
    {
        return -1;
    }
    else
    {
        perror("[config] sscanf failed");
        return -1;
    }

    return 0;
}

static int
loadconfig()
{
    int error;
    int nopt = 0;
    size_t maxlen = 300;
    char fline[maxlen];

    FILE* cfile = fopen(g_conf.config_file, "r");
    if(NULL == cfile)
    {
        fprintf(stderr, "[config] Could not open the configuration file"
                " \"%s\"\n\t ", g_conf.config_file);
        perror("");
        return -1;
    }

    if(0 == (error = allocopts()))
    {
        while(NULL != fgets(fline, maxlen, cfile))
        {
            if(0 != (error = readargs(fline, &nopt)))
            {
                fprintf(stderr, "[config] Error in the line: %s", fline);
                break;
            }
        }
        if(0 != (error = ferror(cfile)))
        {
            perror("[config] An error occurred while loading a config");
        }
    }
    fclose(cfile);

    if(0 != error)
    {
        return error;
    }

    g_conf.opts[++nopt] = NULL;
    return cfgmngr(nopt, g_conf.opts);
}

int
recfgmngr()
{
    if(NULL != g_conf.opts)
    {
        free(g_conf.opts);
        g_conf.opts = NULL;
    }
    if(NULL == g_conf.config_file)
    {
        fprintf(stderr,
                "[config] Reconfiguration has been requested, "
                "but no path was specified on server\'s startup\n");
        return -1;
    }
    char* p = g_conf.config_file;
    memset(&g_conf, 0, sizeof(g_conf));
    g_conf.config_file = p;

    return loadconfig();
}

int
cfgmngr(int argc, char** argv)
{
    int opt;
    char reload = 0;

    while(-1 != (opt = getopt_long(argc, argv, OPTSTRING, g_lopts, NULL)))
    {
        switch(opt)
        {
            case 1:
                if(NULL == g_conf.log_path)
                    g_conf.log_path = optarg;
                break;
            case 2:
                if(NULL == g_conf.port)
                    g_conf.port = optarg;
                break;
            case 3:
                if(NULL == g_conf.host)
                    g_conf.host = optarg;
                break;
            case 'l':
                if(NULL == g_conf.log_path)
                    g_conf.log_path = DEF_NO_LOG;
                break;
            case 'r':
                if(NULL == g_conf.document_root)
                    g_conf.document_root = optarg;
                break;
            case 'i':
                if(NULL == g_conf.index_page)
                    g_conf.index_page = optarg;
                break;
            case 'u':
                if(NULL == g_conf.user)
                    g_conf.user = optarg;
                break;
            case 'g':
                if(NULL == g_conf.group)
                    g_conf.group = optarg;
                break;
            case 'v':
                printf("WebServer %s\n", WEB_SERVER_VERSION);
                exit(EXIT_SUCCESS);
            case 'h':
                printhelp();
                exit(EXIT_SUCCESS);
            default: // '?'
                printhelp();
                exit(EXIT_FAILURE);
        }
    }

    if(optind < argc)
    {
        g_conf.config_file = argv[optind++];
        reload = 1;
    }
    optind = 1;

    if(0 == reload)
    {
        if(NULL == g_conf.document_root)
        {
            fprintf(stderr, "[config] You have to specify "
                    "the document-root parameter\n");
            return -1;
        }
        if(NULL == g_conf.index_page)
            g_conf.index_page = DEF_INDEX_PAGE;
        if(NULL == g_conf.log_path)
            g_conf.log_path = DEF_LOG_PATH;
        if(NULL == g_conf.port)
            g_conf.port = DEF_PORT;
        if(NULL == g_conf.host)
            g_conf.host = DEF_HOST;

        if(NULL == g_conf.user || NULL == g_conf.group)
        {
            if(g_conf.user != g_conf.group)
            {
                printf("[config] You have provided either --user or --group,"
                        "but you should use them both\n");
            }
            printf("[config] You can improve security of the server by using "
                    "--user AND --group\n\t arguments. In this case the "
                    "process will drop its root\n\t privileges after "
                    "initialization\n");
            if(NULL == g_conf.user)
                g_conf.user = DEF_USER_NAME;
            if(NULL == g_conf.group)
                g_conf.group = DEF_GROUP_NAME;
        }
        
        return 0;
    }

    return loadconfig();
}
