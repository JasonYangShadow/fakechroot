/*
   libfakechroot -- fake chroot environment
   Copyright (c) 2010, 2013, 2016 Piotr Roszatycki <dexter@debian.org>

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

#include <string.h>
#include "libfakechroot.h"
#include "getcwd_real.h"
#include "unionfs.h"
#include "rel2abs.h"


/**
  wrapper(chdir, int, (const char * path))
  {
  char cwd[FAKECHROOT_PATH_MAX];
  const char *fakechroot_base = getenv("FAKECHROOT_BASE");

  debug("chdir(\"%s\")", path);

  if (getcwd_real(cwd, FAKECHROOT_PATH_MAX) == NULL) {
  return -1;
  }
  if (fakechroot_base != NULL) {
  if (strstr(cwd, fakechroot_base) == cwd) {
  expand_chroot_path(path);
  }
  else {
  expand_chroot_rel_path(path);
  }
  }

  return nextcall(chdir)(path);
  }
 **/

wrapper(chdir, int, (const char * path))
{
    char resolved[MAX_PATH];
    rel2absLayer(path, resolved);

    if(pathExcluded(path)){
        return nextcall(chdir)(resolved);
    }

    if(lxstat(path) && is_file_type(resolved,TYPE_LINK)){
        char link[FAKECHROOT_PATH_MAX];
        if(resolveSymlink(resolved,link)){
            debug("chdir %s",link);
            return nextcall(chdir)(link);
        }
    }
    if(!is_inside_container(resolved)){
        debug("chdir %s escapes from container, we have to fix it by force");
        const char * container_root = getenv("ContainerRoot");
        strcpy(resolved, container_root);
    }
    debug("chdir %s",resolved);
    return nextcall(chdir)(resolved);
}
