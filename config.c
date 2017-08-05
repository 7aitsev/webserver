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
        "--user [-u]            : Change the user id of the process\n"
        "--group [-g]           : Change the group id of the process\n"
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
    CONFIG.user = NULL;
    CONFIG.group = NULL;

    const struct option lopts[] = {
        {"index-page", optional_argument, NULL, 'i'},
        {"user", optional_argument, NULL, 'u'},
        {"group", optional_argument, NULL, 'g'}, 
        {"help", no_argument, NULL, 'h'},
        {"version", no_argument, NULL, 'v'}
    };

    int opt;
    while(-1 != (opt = getopt_long(argc, argv, "vi:u:g:", lopts, NULL)))
    {
        switch(opt)
        {
            case 'i':
                CONFIG.directory_index = optarg;
                break;
            case 'u':
                CONFIG.user = optarg;
                break;
            case 'g':
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
        CONFIG.document_root = argv[optind++];
    }
    else
    {
        fprintf(stderr, "You have to specify the document_root parameter!\n");
    }

    if(NULL == CONFIG.directory_index)
    {
        CONFIG.directory_index = DIRECTORY_INDEX;
    }

    if(NULL == CONFIG.user || NULL == CONFIG.group)
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

    return 0;
}
