#include <config.h>

#include "libfakechroot.h"
#include "unionfs.h"

wrapper(fdopendir, DIR*, (int fd))
{
    char path[MAX_PATH];
    debug("fdopendir starts on fd:%d", fd);
    get_path_from_fd(fd, path);
    size_t num;
    struct dirent_obj* darr = NULL;
    DIR* dirp = getDirents(path, &darr, &num);
    if(darr_list[fd] != NULL){
        clearItems(&darr_list[fd]);
    }

    clearItems(&darr);
    darr = WRAPPER_FUFS(opendir, opendir, path);
    fd = dirfd(dirp);
    darr_list[fd] = darr;
    debug("fdopendir save fd: %d on %s to darr list", fd, path);
    return dirp;
}
