#include "config.h"

struct config CONFIG;

int
config(int argc, char** argv)
{
    const struct option lopts[] = {
        {"index-page", optional_argument, NULL, 'i'},
        {"version", no_argument, NULL, 'v'}
    };

    int opt;
    int opt_idx = 0;
    while(-1 != (opt = getopt_long(argc, argv, "vi:", lopts, &opt_idx)))
    {
        switch(opt)
        {
            case 'i':
                printf("index = %s\n", optarg);
                break;
            case 'v':
                printf("WebServer %s\n", WEB_SERVER_VERSION);
                return -1;
            default: // '?'
                printf(
"WebServer %s\n\n"
"Usage: webserver [arguments] [document_root]\n"
"       start the server in a specified root directory.\n\n"
"Arguments:\n"
"--directory-index [-i] : Set default home page path\n"
"--version [-v]         : Print version information and exit\n",
                        WEB_SERVER_VERSION
                    );
                return -1;
        }
    }

    if(optind < argc)
    {
        while(optind < argc) {
            printf("args: %s\n", argv[optind++]);
        }
//        CONFIG.document_root = argv[opt_idx++];
//        printf("document_root = %s\n", CONFIG.document_root);
    }

    return 0;
}
