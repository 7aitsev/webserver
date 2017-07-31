#include "config.h"

struct config CONFIG;

void
printhelp()
{
    printf(
        "WebServer %s\n\n"
        "Usage: webserver [arguments] [document_root]\n"
        "       start the server in a specified root directory.\n\n"
        "Arguments:\n"
        "--directory-index [-i] : Set a home page path (default is `%s`)\n"
        "--help [-h]            : Print help (this message) and exit\n"
        "--version [-v]         : Print version information and exit\n",
        WEB_SERVER_VERSION,
        DIRECTORY_INDEX
    );
}

int
init_server(int argc, char** argv)
{
    CONFIG.directory_index = NULL;
    CONFIG.document_root = NULL;

    const struct option lopts[] = {
        {"index-page", optional_argument, NULL, 'i'},
        {"help", no_argument, NULL, 'h'},
        {"version", no_argument, NULL, 'v'}
    };

    int opt;
    int opt_idx = 0;
    while(-1 != (opt = getopt_long(argc, argv, "vi:", lopts, &opt_idx)))
    {
        switch(opt)
        {
            case 'i':
                CONFIG.directory_index = optarg;
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
        CONFIG.document_root = argv[optind++];
    }

    if(NULL == CONFIG.directory_index)
    {
        CONFIG.directory_index = DIRECTORY_INDEX;
    }

    if(NULL == CONFIG.document_root)
    {
        fprintf(
            stderr,
            "You have to specify the document_root parameter!\n"
            );
        return -1;
    }

    return 0;
}
