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

#include <stddef.h>
#include "libfakechroot.h"
#include "unionfs.h"
wrapper(getcwd, char *, (char * buf, size_t size))
{
    char * cwd;
    INITIAL_SYS(getcwd)

    debug("getcwd(%p, %zd)", buf, size);
    if ((cwd = real_getcwd(buf, size)) == NULL){
        debug("could not get cwd, return NULL");
        return NULL;
    }

    char rel_path[MAX_PATH];
    char layer_path[MAX_PATH];
    int ret = get_relative_path_layer(cwd, rel_path, layer_path);
    if(ret != 0){
        debug("current cwd is not inside container %s",cwd);
        errno = EACCES;
        return NULL;
    }

    memset(cwd,'\0',strlen(cwd));
    if(strcmp(rel_path,".") == 0){
        strcpy(cwd,"/");
    }else{
        size_t len = strlen(rel_path) + 2;
        snprintf(cwd,len,"/%s",rel_path);
    }
    buf = cwd;
    return cwd;
}

/**
  wrapper(getcwd, char *, (char * buf, size_t size))
  {
  char *cwd;

  debug("getcwd(&buf, %zd)", size);
  if ((cwd = nextcall(getcwd)(buf, size)) == NULL) {
  return NULL;
  }
  narrow_chroot_path(cwd);
  return cwd;
  }
 **/
