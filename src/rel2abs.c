/*
   libfakechroot -- fake chroot environment
   Copyright (c) 2013 Piotr Roszatycki <dexter@debian.org>

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

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "libfakechroot.h"
#include "strlcpy.h"
#include "dedotdot.h"
#include "getcwd_real.h"
#include "unionfs.h"
#include "hmappriv.h"

/**
LOCAL char * rel2abs(const char * name, char * resolved)
{
    char cwd[FAKECHROOT_PATH_MAX];

    if (name == NULL) {
        resolved = NULL;
        goto end;
    }

    if (*name == '\0') {
        *resolved = '\0';
        goto end;
    }

    getcwd_real(cwd, FAKECHROOT_PATH_MAX); narrow_chroot_path(cwd);

    if (*name == '/') {
        strlcpy(resolved, name, FAKECHROOT_PATH_MAX);
    }else {
        snprintf(resolved, FAKECHROOT_PATH_MAX, "%s/%s", cwd, name);
    }

    dedotdot(resolved);


end:
    debug("rel2abs(\"%s\", \"%s\")", name, resolved);
    return resolved;
}
**/

LOCAL char * rel2absLayer(const char * name, char * resolved){
    int errsv = errno;
    char cwd[FAKECHROOT_PATH_MAX];

    debug("rel2absLayer starts(\"%s\", &resolved)", name);
    if (name == NULL){
        resolved = NULL;
        goto end;
    }
    if (*name == '\0') {
        *resolved = '\0';
        goto end;
    }

    getcwd_real(cwd, FAKECHROOT_PATH_MAX);
    debug("rel2absLayer current cwd: %s", cwd);

    //preprocess name
    char name_dup[FAKECHROOT_PATH_MAX];
    strcpy(name_dup, name);
    dedotdot(name_dup);

    const char * container_root = getenv("ContainerRoot");
    const char * exclude_ex_path = getenv("FAKECHROOT_EXCLUDE_EX_PATH");
    if (*name_dup == '/') {
        //if the path is absolute path
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
            //we check if existing in other layers
            if(!findFileInLayers(name_dup, resolved)){
                //before redirecting path into container by force, let us check if it exists in sys lib path and patch path
                const char * syslib_path = getenv("FAKECHROOT_SyslibPath");
                const char * patch_path = getenv("FAKECHROOT_LDPatchPath");
                if(syslib_path && *syslib_path != '\0'){
                   if(strncmp(name_dup, syslib_path, strlen(syslib_path)) == 0){
                       debug("rel2abs path:%s is included in syslib_path:%s", name_dup, syslib_path);
                       strlcpy(resolved, name_dup, FAKECHROOT_PATH_MAX);
                       goto end;
                   }
                }
                if(patch_path && *patch_path != '\0'){
                    if(strncmp(name_dup, patch_path, strlen(patch_path)) == 0){
                        debug("rel2abs path:%s is included in patch_path:%s", name_dup, patch_path);
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
                    debug("rel2abs path: %s is included in remote exec map: %s", name_dup, replace_path_p);
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
    }else {
        //if the path is relative path
        char tmp[FAKECHROOT_PATH_MAX];
        sprintf(tmp,"%s/%s",cwd,name_dup);

        char rel_path[FAKECHROOT_PATH_MAX];
        char layer_path[FAKECHROOT_PATH_MAX];
        //test if the path exists in container layers
        int ret = get_relative_path_layer(tmp, rel_path, layer_path);
        bool b_resolved = false;
        debug("rel2abs get relative path info, path: %s, rel_path: %s, layer_path: %s, ret: %d", tmp, rel_path, layer_path, ret);
        if(ret == 0){
            //exists?  here we have to use lxstat because some files are symlinks
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
            char old_path[MAX_PATH];
            strcpy(old_path, resolved);
            memset(resolved,'\0',MAX_PATH);
            if(!findFileInLayers(old_path, resolved)){
                debug("rel2absLayer could not resolve escaped path: %s", old_path);
            }
        }

        dedotdot(resolved);
        if(pathExcluded(resolved)){
            goto end;
        }
        if(!is_inside_container(resolved)){
            debug("rel2absLayer path: %s escape from container, we have to fix it by force", resolved);
            char fix_tmp[MAX_PATH];
            snprintf(fix_tmp, FAKECHROOT_PATH_MAX, "%s%s", container_root, resolved);
            memset(resolved,'\0',MAX_PATH);
            strcpy(resolved, fix_tmp);
        }
    }

end:
    dedotdot(resolved);
    debug("rel2absLayer ends(\"%s\", \"%s\")", name_dup, resolved);
    errno = errsv;
    return resolved;
}
