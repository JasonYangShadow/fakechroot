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

#ifdef HAVE_DLMOPEN

#define _GNU_SOURCE
#include <dlfcn.h>
#include "libfakechroot.h"
#include "unionfs.h"

wrapper(dlmopen, void *, (Lmid_t nsid, const char * filename, int flag))
{
    debug("dlmopen(&nsid, \"%s\", %d)", filename, flag);
    if(filename && *filename == '/'){
        expand_chroot_path(filename);
    }
    char *ld = getenv("LD_LIBRARY_PATH");
    if(ld){
        debug("dlmopen with LD_LIBRARY_PATH: %s", ld);
    }else{
        debug("dlmopen with NO LD_LIBRARY_PATH set!!");
    }
    //process ld_library_path before dlopen in order to patch ld_library_path
    fakechroot_merge_ld_path(NULL);

    if(!filename){
        return nextcall(dlmopen)(nsid, filename, flag);
    }

    Queue *q = NULL;
    Stack *st = NULL;
    bool bret = InitializeStack(STACK_DEFAULT_MAX_SIZE, &st);
    QueuePush(&q, filename);
    debug("dlmopen starts processing %s's dependency tree", filename);
    int ret = gen_tree_dependency(q, st);
    if (ret == 0){
        INITIAL_SYS(dlmopen)
        char tmp[PATH_MAX];
        while(StackPop(st, tmp)){
            debug("dlmopen starts loading other libraries: %s", tmp);
            real_dlmopen(nsid, tmp, flag);
        }
    }
    debug("dlmopen return as normal: %s", filename);
    return nextcall(dlmopen)(nsid, filename, flag);
}

#else
typedef int empty_translation_unit;
#endif
