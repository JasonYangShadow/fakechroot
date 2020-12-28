#ifndef __FILECACHE_CLIENT_H
#define __FILECACHE_CLIENT_H
#define FCMAX_PATH 4096
#include <stdbool.h>

bool getFCValue(const char* key, char *value);
#endif
