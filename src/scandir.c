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

#ifdef HAVE_SCANDIR

#include <dirent.h>
#include "libfakechroot.h"
#include "unionfs.h"
#include <stdlib.h>

int compare(const void* A, const void* B){
    return strcmp((*(struct dirent**)A)->d_name, (*(struct dirent **)B)->d_name);
}

wrapper(scandir, int, (const char * dir, struct dirent *** namelist, SCANDIR_TYPE_ARG3(filter), SCANDIR_TYPE_ARG4(compar)))
{
    debug("scandir(\"%s\", &namelist, &filter, &compar)", dir);
    expand_chroot_path(dir);

    if(pathExcluded(dir)){
        return nextcall(scandir)(dir, namelist, filter, compar);
    }

    int num;
    struct dirent_obj* ret = listDir(dir, &num);
    if(num > 0 && ret != NULL){
        *namelist = (struct dirent **)malloc(sizeof(struct dirent *)*num);
        struct dirent_obj* loop = ret;
        int i = 0;
        while(loop != NULL){
            if(filter!=NULL && !(*filter)(loop->dp)){
                loop = loop->next;
                continue;
            }
            *namelist[i] = loop->dp;
            loop = loop->next;
            i++;
        }
        if(num > 0 && compar != NULL){
            qsort(*namelist, num, sizeof(struct dirent *), compare);
        }
        clearItems(&ret);
        return i;
    }else{
        if(ret != NULL){
            clearItems(&ret);
        }
        *namelist = NULL;
        return 0;
    }
}

/**
  wrapper(scandir, int, (const char * dir, struct dirent *** namelist, SCANDIR_TYPE_ARG3(filter), SCANDIR_TYPE_ARG4(compar)))
  {
  debug("scandir(\"%s\", &namelist, &filter, &compar)", dir);
  expand_chroot_path(dir);
  return nextcall(scandir)(dir, namelist, filter, compar);
  }
 **/

#else
typedef int empty_translation_unit;
#endif
