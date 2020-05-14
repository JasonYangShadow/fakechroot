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

#ifdef HAVE_READDIR64

#define _LARGEFILE64_SOURCE
#include <dirent.h>
#include "libfakechroot.h"
#include "unionfs.h"

extern struct dirent_obj* darr_list[MAX_ITEMS];
wrapper(readdir64, struct dirent64 *, (DIR * dirp))
{
    int fd = dirfd(dirp);
    struct dirent_obj* darr = darr_list[fd];
    debug("readdir64 file descriptor: %d", fd);
    if(darr != NULL){
        struct dirent64* entry64 = popItemFromHeadV64(&darr);
        debug("readdir64 %s", entry64->d_name);
        darr_list[fd] = darr;
        return entry64;
    }else{
        debug("default readdir64");
        return NULL;
    }
}
#else
typedef int empty_translation_unit;
#endif
