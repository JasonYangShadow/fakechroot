#include "util.h"
#include <sys/time.h>
#include <sys/resource.h>
#include <malloc.h>
#include <stdio.h>

long getMemCurrProcess(){
    struct rusage rs;
    int ret = getrusage(RUSAGE_SELF,&rs);
    if(ret == 0){
        return rs.ru_maxrss;
    }
    return -1;
}

long getMemChilden(){
    struct rusage rs;
    int ret = getrusage(RUSAGE_CHILDREN,&rs);
    if(ret == 0){
        return rs.ru_maxrss;
    }
    return -1;
}

void* debug_malloc_expand(size_t size, char* file, int line){
    void* ret = malloc(size);
    const char* MALLOC_DEBUG = getenv("FAKECHROOT_MALLOC_DEBUG");
    if(MALLOC_DEBUG){
        fprintf(stderr, ">>> file:%s, line: %d, size: %ld, address: %p \n", file, line, size, ret);
    }
    return ret;
}

void debug_free_expand(void *ptr, char* file, int line){
    const char* MALLOC_DEBUG = getenv("FAKECHROOT_MALLOC_DEBUG");
    if(MALLOC_DEBUG){
        fprintf(stderr, "<<< file:%s, line: %d, address: %p \n", file, line, ptr);
    }
    free(ptr);
}

void* debug_realloc_expand(void*ptr, size_t size, char* file, int line){
    void* ret = realloc(ptr, size);
    const char* MALLOC_DEBUG = getenv("FAKECHROOT_MALLOC_DEBUG");
    if(MALLOC_DEBUG){
        fprintf(stderr, ">>> file:%s, line: %d, realloc_size: %ld, address: %p \n", file, line, size, ret);
    }
    return ret;
}
