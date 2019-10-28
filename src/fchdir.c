#include <config.h>

#include "libfakechroot.h"
#include "unionfs.h"
#include <unistd.h>

wrapper(fchdir, int, (int fd))
{
    debug("fchdir %d", fd);
    /**
    DIR *f = fdopendir(fd);
    INITIAL_SYS(readdir)
    struct dirent *entry;
    while(entry = real_readdir(f)){
        printf("---- %s\n", entry->d_name);
    }
    **/
    return nextcall(fchdir)(fd);
}
