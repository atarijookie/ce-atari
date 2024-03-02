#ifndef EXTENSIONRECVTHREAD_H
#define EXTENSIONRECVTHREAD_H

#include <string.h>
#include <stdio.h>
#include <stdio.h>
#include <string.h>

int createRecvSocket(const char* dotEnvKey);
void *extensionThreadCode(void *ptr);

#endif
