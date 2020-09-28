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
#define _ATFILE_SOURCE
#include "libfakechroot.h"
#include "unionfs.h"

wrapper(renameat2, int, (int olddirfd, const char * oldpath, int newdirfd, const char * newpath, unsigned int flags))
{
    char tmp[FAKECHROOT_PATH_MAX];
    debug("renameat2(%d, \"%s\", %d, \"%s\", %d)", olddirfd, oldpath, newdirfd, newpath, flags);
    expand_chroot_path_at(olddirfd, oldpath);
    strcpy(tmp, oldpath);
    oldpath = tmp;
    expand_chroot_path_at(newdirfd, newpath);
    
    char** rt_paths;
    bool r = rt_mem_check("renameat", 2, &rt_paths, oldpath, newpath);
    if (r && rt_paths){
      return nextcall(renameat2)(olddirfd, rt_paths[0], newdirfd, rt_paths[1], flags);
    }else{
      return WRAPPER_FUFS(rename,renameat2,olddirfd, oldpath, newdirfd, newpath, flags)
    }
}
