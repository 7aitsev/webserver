#ifndef CONFIG_H
#define CONFIG_H

#include <stdio.h>
#include <getopt.h>

#define WEB_SERVER_VERSION "0.9"

struct config
{
    char* directory_index;
    char* document_root;
};

extern struct config CONFIG;

int
config(int argc, char** argv);

#endif
