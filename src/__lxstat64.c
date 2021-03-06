/*
    libfakechroot -- fake chroot environment
    Copyright (c) 2010, 2013, 2015 Piotr Roszatycki <dexter@debian.org>

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

#ifdef HAVE___LXSTAT64

#define _LARGEFILE64_SOURCE
#define _XOPEN_SOURCE 500
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include "libfakechroot.h"
#include "readlink.h"
#include "unionfs.h"

wrapper(__lxstat64, int, (int ver, const char * filename, struct stat64 * buf))
{
    int errsv = errno;
    char tmp[FAKECHROOT_PATH_MAX];
    int retval;
    READLINK_TYPE_RETURN linksize;
    
    debug("__lxstat64(%d, \"%s\", &buf)", ver,filename);

    char orig[FAKECHROOT_PATH_MAX];
    strcpy(orig, filename);

    expand_chroot_path(filename);
    //here if path inside container does not exist && path inside host does not exist neither, return container info
    errno = errsv;
    retval = nextcall(__lxstat64)(ver, filename, buf);
    if(retval != 0){
        int old_retval = retval;
        retval = nextcall(__lxstat64)(ver, orig, buf);
        if(retval == 0){
            filename = orig;
        }else{
            //redo to revert to container path
            retval = nextcall(__lxstat64)(ver, filename, buf);
        }
    }
    //original bug fix but have to be changed in our case
    //if target is symlink have to modify the link size
    if((retval == 0) && (buf->st_mode & S_IFMT) == S_IFLNK){
        INITIAL_SYS(readlink)
        if((linksize = real_readlink(filename, tmp, sizeof(tmp) - 1)) != -1){
            buf->st_size = linksize;
        }
    }
    return retval;
}


/* Prevent looping with realpath() */
LOCAL int __lxstat64_rel(int ver, const char * filename, struct stat64 * buf)
{
    char tmp[FAKECHROOT_PATH_MAX];
    int retval;
    READLINK_TYPE_RETURN linksize;
    debug("__lxstat64_rel(%d, \"%s\", &buf)", ver, filename);
    retval = nextcall(__lxstat64)(ver, filename, buf);
    /* deal with http://bugs.debian.org/561991 */
    if ((retval == 0) && (buf->st_mode & S_IFMT) == S_IFLNK){
        INITIAL_SYS(readlink)
        if ((linksize = real_readlink(filename, tmp, sizeof(tmp)-1)) != -1)
            buf->st_size = linksize;
    }
    return retval;
}

//wrapper(__lxstat64, int, (int ver, const char * filename, struct stat64 * buf))
//{
//    debug("__lxstat64 %d, %s", ver, filename);
//    return __lxstat64_rel(ver, filename, buf);
//}
//
//LOCAL int __lxstat64_rel(int ver, const char * filename, struct stat64 * buf)
//{
//    char resolved[FAKECHROOT_PATH_MAX];
//    debug("__lxstat64_rel %d, %s", ver, filename);
//    if(*filename != '/'){
//        char cwd[FAKECHROOT_PATH_MAX];
//        getcwd_real(cwd, FAKECHROOT_PATH_MAX);
//        sprintf(resolved, "%s/%s", cwd, filename);
//    }else{
//        strcpy(resolved, filename);
//    }
//    dedotdot(resolved);
//    debug("__lxstat64_rel path: %s, resolved: %s", filename, resolved);
//    return nextcall(__lxstat64)(ver, resolved, buf);
//}

#else
typedef int empty_translation_unit;
#endif
