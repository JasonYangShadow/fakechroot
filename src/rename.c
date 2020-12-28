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

#include "libfakechroot.h"
#include "unionfs.h"

wrapper(rename, int, (const char * oldpath, const char * newpath))
{
    int errsv = errno;
    char tmp[FAKECHROOT_PATH_MAX];
    debug("rename(\"%s\", \"%s\")", oldpath, newpath);
    expand_chroot_path(oldpath);
    strcpy(tmp, oldpath);
    oldpath = tmp;
    expand_chroot_path(newpath);

    char** rt_paths;
    bool r = rt_mem_check("rename", 2, &rt_paths, oldpath, newpath);
    if (r && rt_paths){
      errno = errsv;
      return nextcall(rename)(rt_paths[0], rt_paths[1]);
    }else{
      int ret = WRAPPER_FUFS(rename,rename, oldpath, newpath)
      if(ret == 0){
        errno = errsv;
      }
      return ret;
    }
}
