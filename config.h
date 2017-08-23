#ifndef CONFIG_H
#define CONFIG_H

#define WEB_SERVER_VERSION "0.9"
#define INDEX_PAGE "/index.html" 

struct config
{
    char* config_file;
    char* index_page;
    char* document_root;
    char* user;
    char* group;
    char** opts;
};

extern struct config CONFIG;

int
allocopts();

int
readargs(const char* fline, int * const nopt);

int
loadconfig();

void
printhelp();

int
reconfigure_server();

int
configure(int argc, char** argv);

int
init_server(int argc, char** argv);

#endif
