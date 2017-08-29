#ifndef CONFIG_H
#define CONFIG_H

#include "config/conf.h"

#define WEB_SERVER_VERSION "0.9"
#define INDEX_PAGE "/index.html" 

int
recfgmngr();

int
cfgmngr(int argc, char** argv);

int
cfgserv(int argc, char** argv);

#endif
