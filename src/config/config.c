#include "config/config.h"
#include "config/conf.h"

#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WEB_SERVER_VERSION "1.0"
#define DEF_INDEX_PAGE "/index.html" 
#define DEF_LOG_PATH_OPT "/tmp/webserver.log"
#define DEF_LOG_PATH "/dev/null"
#define DEF_PORT "http"
#define DEF_HOST NULL
#define DEF_USER_NAME "root"
#define DEF_GROUP_NAME "root"

#define OPTSTRING "vhr:i:u:g:"
static const struct option g_lopts[] = {
    {"document-root", required_argument, NULL, 'r'},
    {"index-page", required_argument, NULL, 'i'},
    {"log", optional_argument, NULL, 1},
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

struct conf g_conf;

static int
allocopts()
{
    int nargs = 2 * sizeof(g_lopts) / sizeof(struct option);
    int arglen = 257;
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
        return EXIT_SUCCESS;
    }
    else
    {
        perror("[config] malloc for g_conf.opts failed");
        return EXIT_FAILURE;
    }
}

static int
isstrblank(const char* s)
{
    while(*s != '\0')
    {
        if(!isspace(*s))
            return 1;
        ++s;
    }
    return 0;
}

static int
readargs(const char* fline, int * const nopt)
{
    char param[16]; // 15+1
    char arg[257]; // 256+1
    param[15] = '\0';
    arg[256] = '\0';

    if(0 == isstrblank(fline))
        return EXIT_SUCCESS;

    int rv;
    if(2 == (rv = sscanf(fline, "%15[-a-z] %256s", param, arg)))
    {
        if(NULL == strstr(g_params, param))
        {
            fprintf(stderr, "[config] Parameter \"%s\" in the config"
                    "is invalid\n", param);
            return EXIT_FAILURE;
        }
        sprintf(g_conf.opts[++(*nopt)], "--%s", param);
        strcpy(g_conf.opts[++(*nopt)], arg);
    }
    else if(0 <= rv && 0 == errno)
    {
        if(NULL != strstr("log", param))
        {    
            return EXIT_FAILURE;
        }
        else
        {
            strcpy(g_conf.opts[++(*nopt)], "--log");
        }
    }
    else
    {
        perror("[config] fscanf failed");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
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
        return EXIT_FAILURE;
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
        if(ferror(cfile))
        {
            perror("[config] Some error occurred while loading configuration");
            error = -1;
        }
    }
    fclose(cfile);

    if(0 != error)
    {
        return EXIT_FAILURE;
    }

    g_conf.opts[++nopt] = NULL;
    return cfgmngr(nopt, g_conf.opts);
}

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
"-i, --index-page index      : Set a home page path (default is `%s`)\n"
"--log [log_path]            : Set a path for a log file\n"
"--port port_num             : A port on which the server will listen\n"
"--host host                 : Listen for the given IP adress\n"
"-u, --user user             : Change the user id of the process\n"
"-g, --group group           : Change the group id of the process\n"
"-h, --help                  : Print help (this message) and exit\n"
"-v, --version               : Print version information and exit\n",
            WEB_SERVER_VERSION,
            DEF_INDEX_PAGE
        );
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
        return EXIT_FAILURE;
    }
    char* p = g_conf.config_file;
    memset(&g_conf, 0, sizeof(g_conf));
    g_conf.config_file = p;

    return loadconfig();
}

void
checkopts(int argc, char** argv)
{
    int opt;
    while(-1 != (opt = getopt_long(argc, argv, OPTSTRING, g_lopts, NULL)))
    {
        switch(opt)
        {
            case 1:
            case 2:
            case 3:
            case 'r':
            case 'i':
            case 'u':
            case 'g':
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
    optind = 1;
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
                    g_conf.log_path = (optarg == NULL) ? DEF_LOG_PATH_OPT : optarg;
                break;
            case 2:
                if(NULL == g_conf.port)
                    g_conf.port = optarg;
                break;
            case 3:
                if(NULL == g_conf.host)
                    g_conf.host = optarg;
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
            case 'h':
                break;
            default: // '?'
                return EXIT_FAILURE;
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
            fprintf(stderr, "[config] You have to specify"
                    "the document-root parameter\n");
            return EXIT_FAILURE;
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
        
        return EXIT_SUCCESS;
    }

    return loadconfig();
}

int
cfgserv(int argc, char** argv)
{
    int opt;
    const struct option lopts[] = {
       {"index-page", required_argument, NULL, 'i'},
       {NULL}
    };

    while(-1 != (opt = getopt_long(argc, argv, "i:", lopts, NULL)))
    {
        switch(opt)
        {
            case 'i':
                g_conf.index_page = optarg;
                break;
            default:
                return -1;
        }
    }

    if(NULL == g_conf.index_page)
        return -1;

    return 0;
}
