#include "config.h"

#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct config CONFIG;

int
allocopts()
{
    CONFIG.opts = malloc(10 * sizeof(char*) + 10 * 257);
    if(NULL != CONFIG.opts)
    {
        int i = 0;
        char *offset = (char*) &CONFIG.opts[10];
        for(; i < 10; ++i, offset += 257)
        {
            CONFIG.opts[i] = offset;
        }
        strcpy(CONFIG.opts[0], "./webserver");
        return 0;
    }
    else
    {
        perror("malloc for CONFIG.opts failed");
        return -1;
    }
}

int
readargs(const char* fline, int * const nopt)
{
    char param[16]; // 15+1
    char arg[257]; // 256+1
    param[15] = '\0';
    arg[256] = '\0';
    const char* params = "document-root index-page user group";

    int rv;
    if(2 == (rv = sscanf(fline, "%15[-a-z] %256s", param, arg)))
    {
        if(NULL == strstr(params, param))
        {
            fprintf(stderr, "Parameter in the config is invalid\n");
            return -1;
        }
        sprintf(CONFIG.opts[++(*nopt)], "--%s", param);
        strcpy(CONFIG.opts[++(*nopt)], arg);
    }
    else if(0 <= rv && 0 == errno)
    {
        fprintf(stderr, "Configuration file input failure");
        return -1;
    }
    else
    {
        perror("fscanf failed");
        return -1;
    }

    return 0;
}

int
loadconfig()
{
    int error;
    int nopt = 0;
    size_t maxlen = 300;
    char fline[maxlen];

    FILE* cfile = fopen(CONFIG.config_file, "r");
    if(NULL == cfile)
    {
        perror("Could not open the configuration file");
        return -1;
    }

    if(0 == (error = allocopts()))
    {
        while(NULL != fgets(fline, maxlen, cfile) &&
                0 == (error = readargs(fline, &nopt)));
        if(ferror(cfile))
        {
            perror("Some error occurred while loading configuration");
            error = -1;
        }
    }
    fclose(cfile);

    if(0 != error)
    {
        return -1;
    }

    CONFIG.opts[++nopt] = NULL;
    return configure(nopt, CONFIG.opts);
}

void
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
"-u, --user user             : Change the user id of the process\n"
"-g, --group group           : Change the group id of the process\n"
"-h, --help                  : Print help (this message) and exit\n"
"-v, --version               : Print version information and exit\n",
            WEB_SERVER_VERSION,
            INDEX_PAGE
        );
}

int
init_server(int argc, char** argv)
{
    CONFIG.config_file = NULL;
    CONFIG.index_page = NULL;
    CONFIG.document_root = NULL;
    CONFIG.user = NULL;
    CONFIG.group = NULL;
    CONFIG.opts = NULL;
    
    return configure(argc, argv);
}

int
reconfigure_server()
{
    if(NULL != CONFIG.opts)
    {
        free(CONFIG.opts);
        CONFIG.opts = NULL;
    }
    if(NULL == CONFIG.config_file)
    {
        fprintf(stderr,
                "Reconfiguration has been requested, but no path "
                "was specified on server\'s startup\n"
               );
        return -1;
    }
    CONFIG.index_page = NULL;
    CONFIG.document_root = NULL;
    CONFIG.user = NULL;
    CONFIG.group = NULL;

    return loadconfig();
}

int
configure(int argc, char** argv)
{
    char reload = 0;

    const struct option lopts[] = {
        {"document-root", required_argument, NULL, 'r'},
        {"index-page", required_argument, NULL, 'i'},
        {"user", required_argument, NULL, 'u'},
        {"group", required_argument, NULL, 'g'}, 
        {"help", no_argument, NULL, 'h'},
        {"version", no_argument, NULL, 'v'},
        {0}
    };

    int opt;
    while(-1 != (opt = getopt_long(argc, argv, "vhr:i:u:g:", lopts, NULL)))
    {
        switch(opt)
        {
            case 'r':
                if(NULL == CONFIG.document_root)
                    CONFIG.document_root = optarg;
                break;
            case 'i':
                if(NULL == CONFIG.index_page)
                    CONFIG.index_page = optarg;
                break;
            case 'u':
                if(NULL == CONFIG.user)
                    CONFIG.user = optarg;
                break;
            case 'g':
                if(NULL == CONFIG.group)
                    CONFIG.group = optarg;
                break;
            case 'v':
                printf("WebServer %s\n", WEB_SERVER_VERSION);
                return -1;
            case 'h':
            default: // '?'
                printhelp();
                return -1;
        }
    }

    if(optind < argc)
    {
        CONFIG.config_file = argv[optind++];
        reload = 1;
        optind = 1;
    }

    if(NULL == CONFIG.document_root && 0 == reload)
    {
        fprintf(stderr, "You have to specify the document-root parameter!\n");
        return -1;
    }

    if(NULL == CONFIG.index_page && 0 == reload)
    {
        CONFIG.index_page = INDEX_PAGE;
    }

    if((NULL == CONFIG.user || NULL == CONFIG.group) && 0 == reload)
    {
        if(CONFIG.user != CONFIG.group)
        {
            printf("You have provided either --user or --group,"
                    "but you should use them both\n");
        }
        printf("You can improve security of the server by using --user AND"
                "--group arguments.\nIn this case the process will drop its "
                "root privileges after initialization.\n"
        );
    }

    return (1 == reload) ? loadconfig() : 0;
}
