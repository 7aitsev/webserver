#ifndef HANDLER_H
#define HANDLER_H

#include <regex.h>
#include <stdio.h>
#include <stdlib.h>

regex_t reg_http_get;

void errorRegex(int errcode, const char* msg);
void prepareRegex();
int parseReq(const char * const req, size_t req_size);
void finishRegex();

#endif
