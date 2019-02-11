#ifndef __HMAPPRIV_H
#define __HMAPPRIV_H
#include "hashmap.h"
#include <unistd.h>
#include <sys/types.h>

#define PNAME_SIZE 48
struct ProcessInfo{
    pid_t pid;
    pid_t ppid;
    pid_t groupid;
    char pname[PNAME_SIZE];
};

bool rt_mem_check(const char* function, int n, char*** rt_paths, ...);
#endif




