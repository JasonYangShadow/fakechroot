#include <config.h>

#include <sys/types.h>
#include <dirent.h>
#include "libfakechroot.h"
#include "unionfs.h"

extern struct dirent_obj * darr;
wrapper(closedir, int, (DIR * dirp))
{
    debug("closedir");
    if(darr){
        debug("closedir releasing preallocaed memory...");
        clearItems(&darr);
        darr = NULL;
    }
    return nextcall(closedir)(dirp);
}
