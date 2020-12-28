/*
   libfakechroot -- fake chroot environment
   Copyright (c) 2013-2015 Piotr Roszatycki <dexter@debian.org>

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

#ifdef HAVE_FCHDIR

#define _BSD_SOURCE
#define _GNU_SOURCE
#define _DEFAULT_SOURCE
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>

#include "libfakechroot.h"
#include "strlcpy.h"
#include "dedotdot.h"
#include "open.h"
#include "unionfs.h"

/**
LOCAL char * rel2absat(int dirfd, const char * name, char * resolved)
{
    int cwdfd = 0;
    char cwd[FAKECHROOT_PATH_MAX];

    debug("rel2absat(%d, \"%s\", &resolved)", dirfd, name);

    if (name == NULL) {
        resolved = NULL;
        goto end;
    }

    if (*name == '\0') {
        *resolved = '\0';
        goto end;
    }

    if (*name == '/') {
        strlcpy(resolved, name, FAKECHROOT_PATH_MAX);
    } else if(dirfd == AT_FDCWD) {
        if (! getcwd(cwd, FAKECHROOT_PATH_MAX)) {
            goto error;
        }
        snprintf(resolved, FAKECHROOT_PATH_MAX, "%s/%s", cwd, name);
    } else {
        if ((cwdfd = nextcall(open)(".", O_RDONLY|O_DIRECTORY)) == -1) {
            goto error;
        }

        if (fchdir(dirfd) == -1) {
            goto error;
        }
        if (! getcwd(cwd, FAKECHROOT_PATH_MAX)) {
            goto error;
        }
        if (fchdir(cwdfd) == -1) {
            goto error;
        }
        (void)close(cwdfd);

        snprintf(resolved, FAKECHROOT_PATH_MAX, "%s/%s", cwd, name);
    }

    dedotdot(resolved);

end:
    debug("rel2absat(%d, \"%s\", \"%s\")", dirfd, name, resolved);
    return resolved;

error:
    if (cwdfd) {
        (void)close(cwdfd);
    }
    resolved = NULL;
    debug("rel2absat(%d, \"%s\", NULL)", dirfd, name);
    return resolved;
}
**/

LOCAL char * rel2absatLayer(int dirfd, const char * name, char * resolved)
{
    int errsv = errno;
    int cwdfd = 0;
    char cwd[FAKECHROOT_PATH_MAX];

    debug("rel2absatLayer starts(%d, \"%s\", &resolved)", dirfd, name);
    if (name == NULL) {
        resolved = NULL;
        goto end;
    }

    if (*name == '\0') {
        *resolved = '\0';
        goto end;
    }

    //preprocess name
    char name_dup[FAKECHROOT_PATH_MAX];
    strcpy(name_dup, name);
    dedotdot(name_dup);

    const char * container_root = getenv("ContainerRoot");
    const char * exclude_ex_path = getenv("FAKECHROOT_EXCLUDE_EX_PATH");
    if (*name_dup == '/') {
        if(pathExcluded(name_dup)){
            //here for some specific exclude paths("/sys/fs/kdbus/*"), we modify the path and redirect it into container rather than host
            bool isfind = false;
            if(exclude_ex_path){
                char exclude_ex_path_dup[MAX_PATH];
                strcpy(exclude_ex_path_dup, exclude_ex_path);
                char *str_tmp = strtok(exclude_ex_path_dup, ":");
                while(str_tmp){
                    if(strncmp(str_tmp, name_dup, strlen(str_tmp)) == 0){
                        sprintf(resolved, "%s%s", container_root, name_dup);
                        isfind = true;
                        break;
                    }
                    str_tmp = strtok(NULL,":");
                }
            }

            if(!isfind){
                strlcpy(resolved, name_dup, FAKECHROOT_PATH_MAX);
            }
        }else{
            if(!findFileInLayers(name_dup, resolved)){
                //before redirecting path into container by force, let us check if it exists in sys lib path and patch path
                const char * syslib_path = getenv("FAKECHROOT_SyslibPath");
                const char * patch_path = getenv("FAKECHROOT_LDPatchPath");
                if(syslib_path && *syslib_path != '\0'){
                   if(strncmp(name_dup, syslib_path, strlen(syslib_path)) == 0){
                       debug("rel2absat path:%s is included in syslib_path:%s", name_dup, syslib_path);
                       strlcpy(resolved, name_dup, FAKECHROOT_PATH_MAX);
                       goto end;
                   }
                }
                if(patch_path && *patch_path != '\0'){
                    if(strncmp(name_dup, patch_path, strlen(patch_path)) == 0){
                        debug("rel2absat path:%s is included in patch_path:%s", name_dup, patch_path);
                        strlcpy(resolved, name_dup, FAKECHROOT_PATH_MAX);
                        goto end;
                    }
                }

                //before redirecting the path into container by force
                //let me check the remote memcached if there are any mappings
                //this one really decreases the performance
                char replace_path[FAKECHROOT_PATH_MAX];
                char* replace_path_p = replace_path;
                bool exec_ok = rt_mem_exec_map(name_dup, &replace_path_p);
                if(exec_ok){
                    debug("rel2absat path: %s is included in remote exec map: %s", name_dup, replace_path_p);
                    strlcpy(resolved, replace_path_p, FAKECHROOT_PATH_MAX);
                    goto end;
                }

                //else we have to redirect the path into container by force
                char rel_path[FAKECHROOT_PATH_MAX];
                char layer_path[FAKECHROOT_PATH_MAX];
                int ret = get_relative_path_layer(name_dup, rel_path, layer_path);
                if(ret == 0){
                    sprintf(resolved,"%s/%s",container_root,rel_path);
                }else{
                    sprintf(resolved,"%s%s",container_root,name_dup);
                }
            }
        }
    } else if(dirfd == AT_FDCWD) {
        if (!getcwd(cwd, FAKECHROOT_PATH_MAX)) {
            goto error;
        }

        //if cwd is excluded path
        if(pathExcluded(cwd)){
            sprintf(resolved,"%s/%s", cwd, name_dup);
            goto end;
        }

        /******************************************/
        char tmp[FAKECHROOT_PATH_MAX];
        if(strcmp(cwd,"/") == 0){
            sprintf(tmp,"%s/%s",container_root, name_dup);
        }else{
            sprintf(tmp,"%s%s/%s",container_root, cwd, name_dup);
        }

        char rel_path[FAKECHROOT_PATH_MAX];
        char layer_path[FAKECHROOT_PATH_MAX];
        //test if the path exists in container layers
        int ret = get_relative_path_layer(tmp, rel_path, layer_path);
        bool b_resolved = false;
        if(ret == 0){
            //exists?
            if(lxstat(tmp)){
                snprintf(resolved,FAKECHROOT_PATH_MAX,"%s",tmp);
                goto end;
            }else{
                //loop to find in each layer
                char ** paths;
                size_t num;
                paths = getLayerPaths(&num);
                if(num > 0){
                    for(size_t i = 0; i< num; i++){
                        char tmp[FAKECHROOT_PATH_MAX];
                        sprintf(tmp, "%s/%s", paths[i], rel_path);
                        if(!xstat(tmp)){
                            //debug("rel2absLayer failed resolved: %s",tmp);
                            if(getParentWh(tmp)){
                                break;
                            }
                            continue;
                        }else{
                            //debug("rel2absLayer successfully resolved: %s",tmp);
                            snprintf(resolved,FAKECHROOT_PATH_MAX,"%s",tmp);
                            b_resolved = true;
                            break;
                        }

                    }
                }
                if(!b_resolved){
                    const char * container_root = getenv("ContainerRoot");
                    snprintf(resolved, FAKECHROOT_PATH_MAX,"%s/%s",container_root,rel_path);
                }
                if(paths){
                    for(size_t i = 0; i < num; i++){
                        free(paths[i]);
                    }
                    free(paths);
                }
            }
        }else{
            snprintf(resolved, FAKECHROOT_PATH_MAX,"%s/%s",cwd,name_dup);
            //add processing fake absolute path
            char old_path[MAX_PATH];
            strcpy(old_path, resolved);
            memset(resolved,'\0',MAX_PATH);
            if(!findFileInLayers(old_path, resolved)){
                debug("rel2absLayerat could not resolve escaped path: %s", old_path);
            }
        }
        /******************************************/
    } else {
        if ((cwdfd = nextcall(open)(".", O_RDONLY|O_DIRECTORY)) == -1) {
            goto error;
        }

        if (fchdir(dirfd) == -1) {
            goto error;
        }
        if (! getcwd(cwd, FAKECHROOT_PATH_MAX)) {
            goto error;
        }
        if (fchdir(cwdfd) == -1) {
            goto error;
        }
        char cwd2[FAKECHROOT_PATH_MAX];
        if (! getcwd(cwd2, FAKECHROOT_PATH_MAX)) {
            goto error;
        }
        (void)close(cwdfd);

        /******************************************/
        char tmp[FAKECHROOT_PATH_MAX];
        if(strcmp(cwd,"/") == 0){
            sprintf(tmp,"%s/%s",container_root, name_dup);
        }else{
            sprintf(tmp,"%s%s/%s",container_root, cwd, name_dup);
        }

        char rel_path[FAKECHROOT_PATH_MAX];
        char layer_path[FAKECHROOT_PATH_MAX];
        //test if the path exists in container layers
        int ret = get_relative_path_layer(tmp, rel_path, layer_path);
        bool b_resolved = false;
        if(ret == 0){
            //exists?
            if(lxstat(tmp)){
                snprintf(resolved,FAKECHROOT_PATH_MAX,"%s",tmp);
                goto end;
            }else{
                //loop to find in each layer
                char ** paths;
                size_t num;
                paths = getLayerPaths(&num);
                if(num > 0){
                    for(size_t i = 0; i< num; i++){
                        char tmp[FAKECHROOT_PATH_MAX];
                        sprintf(tmp, "%s/%s", paths[i], rel_path);
                        if(!lxstat(tmp)){
                            //debug("rel2absLayer failed resolved: %s",tmp);
                            if(getParentWh(tmp)){
                                break;
                            }
                            continue;
                        }else{
                            //debug("rel2absLayer successfully resolved: %s",tmp);
                            snprintf(resolved,FAKECHROOT_PATH_MAX,"%s",tmp);
                            b_resolved = true;
                            break;
                        }

                    }
                }
                if(!b_resolved){
                    const char * container_root = getenv("ContainerRoot");
                    snprintf(resolved, FAKECHROOT_PATH_MAX,"%s/%s",container_root,rel_path);
                }
                if(paths){
                    for(size_t i = 0; i < num; i++){
                        free(paths[i]);
                    }
                    free(paths);
                }
            }
        }else{
            snprintf(resolved, FAKECHROOT_PATH_MAX,"%s/%s",cwd,name_dup);
            //add processing fake absolute path
            char old_path[MAX_PATH];
            strcpy(old_path, resolved);
            memset(resolved,'\0',MAX_PATH);
            if(!findFileInLayers(old_path, resolved)){
                debug("rel2absLayerat could not resolve escaped path: %s", old_path);
            }
        }
        /******************************************/
    }

    dedotdot(resolved);
    if(pathExcluded(resolved)){
        goto end;
    }
    if(!is_inside_container(resolved)){
        debug("rel2absatLayer path: %s escape from container, we have to fix it by force", resolved);
        char fix_tmp[MAX_PATH];
        snprintf(fix_tmp, FAKECHROOT_PATH_MAX, "%s%s", container_root, resolved);
        memset(resolved,'\0',MAX_PATH);
        strcpy(resolved, fix_tmp);
    }

end:
    dedotdot(resolved);
    debug("rel2absatLayer ends(%d, \"%s\", \"%s\")", dirfd, name_dup, resolved);
    return resolved;

error:
    if (cwdfd) {
        (void)close(cwdfd);
    }
    resolved = NULL;
    debug("rel2absatLayer error(%d, \"%s\", NULL)", dirfd, name_dup);
    errno = errsv;
    return resolved;
}

#else
typedef int empty_translation_unit;
#endif
