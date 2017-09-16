#ifndef CONF_H
#define CONF_H

struct conf
{
    char* document_root;
    char* index_page;
    char* log_path;
    char* port;
    char* host;
    char* user;
    char* group;
    char* config_file;
    char** opts;
};

#endif
