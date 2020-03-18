/*
   libfakechroot -- fake chroot environment
   Copyright (c) 2010, 2013 Piotr Roszatycki <dexter@debian.org>
   Copyright (c) 2014 Robin McCorkell <rmccorkell@karoshi.org.uk>

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
#include <dlfcn.h>
#include "unionfs.h"

wrapper(dlopen, void *, (const char * filename, int flag))
{
    debug("dlopen(\"%s\", %d)", filename, flag);
    if (filename && *filename == '/') {
        expand_chroot_path(filename);
    }
    char *ld = getenv("LD_LIBRARY_PATH");
    if(ld){
        debug("dlopen with LD_LIBRARY_PATH: %s", ld);
    }else{
        debug("dlopen with NO LD_LIBRARY_PATH set!!");
    }
    //process ld_library_path before dlopen in order to patch ld_library_path
    fakechroot_merge_ld_path(NULL);

    //if filenmae is NULL, directly return it
    if(!filename){
        return nextcall(dlopen)(filename, flag);
    }

    Queue *q = NULL;
    Stack *st = NULL;
    bool bret = InitializeStack(STACK_DEFAULT_MAX_SIZE, &st);
    QueuePush(&q, filename);
    debug("dlopen starts processing %s's dependency tree", filename);
    int ret = gen_tree_dependency(q, st);
    if(ret == 0){
        INITIAL_SYS(dlopen)
        char tmp[PATH_MAX];
        while(StackPop(st, tmp)){
            debug("dlopen starts loading other libraries: %s",tmp);
            real_dlopen(tmp, flag);
        }
    }
    debug("dlopen return as normal: %s", filename);
    return nextcall(dlopen)(filename, flag);
}
