/*
   libfakechroot -- fake chroot environment
   Copyright (c) 2010, 2013 Piotr Roszatycki <dexter@debian.org>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307  USA
   */


#include <config.h>

#ifdef HAVE_SYMLINKAT

#define _ATFILE_SOURCE
#include "libfakechroot.h"
#include "unionfs.h"
#include "dedotdot.h"
#include "getcwd_real.h"

wrapper(symlinkat, int, (const char * oldpath, int newdirfd, const char * newpath))
{
    debug("symlinkat starts oldpath: %s, newdirfd: %d, newpath: %s", oldpath, newdirfd, newpath);
    char old_resolved[MAX_PATH];
    if(*oldpath == '/'){
        expand_chroot_path(oldpath);
        char rel_path[MAX_PATH];
        char layer_path[MAX_PATH];
        int ret = get_relative_path_layer(oldpath, rel_path, layer_path);
        if(ret == 0){
            sprintf(old_resolved, "%s/%s", layer_path, rel_path);
        }else{
            const char * container_root = getenv("ContainerRoot");
            sprintf(old_resolved, "%s%s", container_root, oldpath);
        }
    }else{
        strcpy(old_resolved, oldpath);
    }
    dedotdot(old_resolved);

    char new_resolved[MAX_PATH];
    if(*newpath == '/'){
        expand_chroot_path(newpath);
        strcpy(new_resolved, newpath);
    }else{
        expand_chroot_path_at(newdirfd, newpath);
        strcpy(new_resolved, newpath);
    }
    dedotdot(new_resolved);

    debug("symlinkat oldpath: %s, newpath: %s, newdirfd: %d", old_resolved, new_resolved, newdirfd);

    return WRAPPER_FUFS(symlink, symlinkat, old_resolved, newdirfd, new_resolved)
}

#else
typedef int empty_translation_unit;
#endif
