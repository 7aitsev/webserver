#ifndef CONFIG_H
#define CONFIG_H

#include "config/conf.h"

int
recfgmngr();

int
cfgmngr(int argc, char** argv);

int
cfgserv(int argc, char** argv);

void
checkopts(int argc, char** argv);

#endif
