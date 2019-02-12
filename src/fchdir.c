#include <config.h>

#include "libfakechroot.h"
#include "unionfs.h"
#include <unistd.h>

wrapper(fchdir, int, (int fd))
{
    debug("fchdir %d", fd);
    return nextcall(fchdir)(fd);
}
