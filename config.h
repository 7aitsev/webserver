#ifndef CONFIG_H
#define CONFIG_H

#include <stdio.h>
#include <getopt.h>

#define WEB_SERVER_VERSION "0.9"
#define DIRECTORY_INDEX "/index.html" 

struct config
{
    char* directory_index;
    char* document_root;
};

extern struct config CONFIG;

void
printhelp();

int
init_server(int argc, char** argv);

#endif
