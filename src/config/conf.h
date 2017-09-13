#ifndef CONF_H
#define CONF_H

struct conf
{
    char* config_file;
    char* index_page;
    char* host;
    char* port;
    char* log_path;
    char* document_root;
    char* user;
    char* group;
    char** opts;
};

#endif
