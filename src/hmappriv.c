#include "hmappriv.h"
#include <stdio.h>
#include <stdlib.h>
#include "log.h"
#include "memcached_client.h"
#include "unionfs.h"

bool get_process_info(struct ProcessInfo* pinfo){
    char pathbase[PNAME_SIZE];
    FILE *fp;
    pinfo->pid = getpid();
    pinfo->ppid = getppid();
    pinfo->groupid = getpgrp();
    strcpy(pathbase,"/proc/");
    sprintf(pathbase+strlen(pathbase),"%d",pinfo->pid);
    sprintf(pathbase+strlen(pathbase),"%s","/exe");

    if(is_file_type(pathbase, TYPE_LINK)){
        INITIAL_SYS(readlink)
            if(real_readlink(pathbase, pinfo->pname, PNAME_SIZE) == -1){
                log_fatal("could not read process info %s", pathbase);
                return false;
            }
        return true;
    }
    log_fatal("%s is not symlink could not read it", pathbase);
    return false;
}

bool rt_mem_exec_map(const char* path, char** n_path){
    char * exec_switch = getenv("FAKECHROOT_EXEC_SWITCH");
    if(!exec_switch){
        return false;
    }
    char * containerid = getenv("ContainerId");
    if(containerid){
        log_debug("start checking exec from remote memcahced, containerid: %s, path: %s", containerid, path);
        char key[MEMCACHED_MAX_KEY];
        //key format: exec:container_id:program path:path
        //eg, exec:id:/usr/bin/vim
        //value real_path against to the host
        sprintf(key, "exec:%s:%s", containerid, path);
        char* value = getValue(key);
        log_debug("query remote memcached with key: %s, return value: %s", key, value);
        if(value != NULL && *value){
            strcpy(*n_path, value);
            return true;
        }
    }

    return false;
}

bool rt_mem_con_check(const char* containerid, const char* function, int n, char** paths, char*** rt_paths){
    char key[MEMCACHED_MAX_KEY];
    //key format: container_id:program full path:system call wrapper
    //eg, id:/bin/bash:open
    //value format: path:replace_path;path:replace_path;
    sprintf(key,"map:%s:%s",containerid,function);
    char* value = getValue(key);
    log_debug("query remote memcached with key: %s, return value: %s", key, value);
    bool b_change = false;
    const char* container_root = getenv("ContainerRoot");
    if(value != NULL){
        *rt_paths = (char**)malloc(sizeof(char*)*n);
        for(int i = 0; i<n; i++){
            b_change = false;
            char* valret = NULL;
            char* rest = value;
            char* token = NULL;
            if(strncmp(container_root, paths[i], strlen(container_root)) == 0){
                if(!lxstat(paths[i])){
                    char tkey[MAX_PATH];
                    sprintf(tkey, "%s:",paths[i]+strlen(container_root));
                    while((token = strtok_r(rest,";",&rest))){
                        //token = current split
                        if((valret = strstr(token, tkey))){
                            char *path = (char *)malloc(sizeof(char)*MAX_PATH);
                            strcpy(path, token+strlen(tkey));
                            log_debug("replacing value, old->%s, new->%s", tkey, path);
                            (*rt_paths)[i] = path;
                            b_change = true;
                            break;
                        }
                    }
                }
            }
            if(!b_change){
                (*rt_paths)[i] = (char *)malloc(sizeof(char)*MAX_PATH);
                strcpy((*rt_paths)[i], paths[i]);
            }
        }//path[i] ends
    }
    //if not changed, release malloc memory
    if(!b_change && value && *rt_paths){
        for(int i = 0; i<n; i++){
            if(*rt_paths && (*rt_paths)[i]){
                free((*rt_paths)[i]);
            }
        }
        if(*rt_paths){
            free(*rt_paths);
        }
        *rt_paths = NULL;
    }
    return b_change;
}

//return: true -> should use replaced path rather than original one
bool rt_mem_check(const char* function, int n, char*** rt_paths, ...){
    char * check_switch = getenv("FAKECHROOT_PRIV_SWITCH");
    if(!check_switch){
        return false;
    }

    /**
      struct ProcessInfo pinfo;
    //could not get process info, let's pass it
    if(!get_process_info(&pinfo)){
    return false;
    }
     **/
    log_debug("dynamical remap feature is enabled");

    char** paths = (char **)malloc(sizeof(char *)*n);
    va_list args;
    va_start(args, rt_paths);
    for(int i=0;i<n;i++){
        paths[i] = va_arg(args,char*);
        //only check absolute path
        if(*paths[i] != '/'){
            return false;
        }
    }
    va_end(args);

    //get container info from envs
    char * containerid = getenv("ContainerId");

    if(containerid){
       log_debug("start checking incoming request, containerid: %s, function: %s", containerid, function);
       return rt_mem_con_check(containerid, function, n, paths, rt_paths);
    }
    log_fatal("fatal error, can't get container id");
    return false;
}
