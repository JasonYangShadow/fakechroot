#ifndef __UTIL_H
#define __UTIL_H
#include <stdlib.h>

long getMemCurrProcess();
long getMemChilden();

#define debug_malloc(s) debug_malloc_expand((s),__FILE__,__LINE__)
void* debug_malloc_expand(size_t size, char* file, int line);
#define debug_free(s) debug_free_expand((s),__FILE__,__LINE__)
void debug_free_expand(void *ptr, char* file, int line);
#define debug_realloc(p,s) debug_realloc_expand((p),(s), __FILE__, __LINE__)
void* debug_realloc_expand(void*ptr, size_t size, char* file, int line);
#endif
