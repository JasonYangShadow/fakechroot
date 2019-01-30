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

#ifdef HAVE_SCANDIR64

#define _LARGEFILE64_SOURCE
#include <dirent.h>
#include "libfakechroot.h"
#include "unionfs.h"
#include <stdlib.h>


int compare64(const void* A, const void* B){
    return strcmp((*(struct dirent64 **)A)->d_name, (*(struct dirent64 **)B)->d_name);
}

wrapper(scandir64, int, (const char * dir, struct dirent64 *** namelist, SCANDIR64_TYPE_ARG3(filter), SCANDIR64_TYPE_ARG4(compar)))
{
    debug("scandir64(\"%s\", &namelist, &filter, &compar)", dir);
    expand_chroot_path(dir);

    int num;
    struct dirent_obj* ret = scanDir(dir, &num, true);
    if(num > 0 && ret != NULL){
        *namelist = (struct dirent64 **)malloc(sizeof(struct dirent64 *)*num);
        struct dirent_obj* loop = ret;
        int i = 0;
        while(loop != NULL){
            if(filter!=NULL && !(*filter)(loop->dp64)){
                loop = loop->next;
                continue;
            }
            *namelist[i] = loop->dp64;
            loop = loop->next;
            i++;
        }
        if(num > 0 && compar != NULL){
            qsort(*namelist, num, sizeof(struct dirent64 *), compare64);
        }
        return i;
    }else{
        *namelist = NULL;
        return 0;
    }
}

/**
  wrapper(scandir64, int, (const char * dir, struct dirent64 *** namelist, SCANDIR64_TYPE_ARG3(filter), SCANDIR64_TYPE_ARG4(compar)))
  {
  debug("scandir64(\"%s\", &namelist, &filter, &compar)", dir);
  expand_chroot_path(dir);
  return nextcall(scandir64)(dir, namelist, filter, compar);
  }
 **/

#else
typedef int empty_translation_unit;
#endif
