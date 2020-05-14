#include <config.h>

#include <sys/types.h>
#include <dirent.h>
#include "libfakechroot.h"
#include "unionfs.h"

extern struct dirent_obj* darr_list[MAX_ITEMS];
wrapper(closedir, int, (DIR * dirp))
{
    int fd = dirfd(dirp);
    debug("closedir fd:%d", fd);
    struct dirent_obj* darr = darr_list[fd];
    if(darr){
        debug("closedir releasing preallocaed memory...");
        clearItems(&darr);
    }
    darr_list[fd] = NULL;
    return nextcall(closedir)(dirp);
}
