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

bool rt_mem_con_check(const char* containerid, const char* pname, const char* function, int n, char** paths, char** rt_paths){
    char key[MEMCACHED_MAX_KEY];
    //key format: container_id:program full path:system call wrapper
    //eg, id:/bin/bash:open
    //value format: path:replace_path;path:replace_path;
    sprintf(key,"%s:%s:%s",containerid,pname, function);
    char* value = getValue(key);
    bool b_change = false;
    if(value != NULL){
        if(rt_paths == NULL){
            rt_paths = (char**)malloc(sizeof(char*)*n);
        }
        for(int i = 0; i<n; i++){
            b_change = false;
            char* valret = NULL;
            char* rest = value;
            char* token = NULL;
            while((token = strtok_r(rest,";",&rest))){
                //token = current split
                char tkey[MAX_PATH];
                sprintf(tkey, "%s:",paths[i]);
                if((valret = strstr(tkey, token))){
                    char *path = (char *)malloc(sizeof(char)*MAX_PATH);
                    strcpy(path, token+strlen(tkey));
                    rt_paths[i] = path;
                    b_change = true;
                    break;
                }
            }
            if(!b_change){
                strcpy(rt_paths[i], paths[i]);
            }
        }//path[i] ends
    }
    //if not changed, release malloc memory
    if(!b_change){
        for(int i = 0; i<n; i++){
            if(rt_paths && rt_paths[i]){
                free(rt_paths[i]);
            }
        }
        if(rt_paths){
            free(rt_paths);
        }
        rt_paths = NULL;
    }
    return b_change;
}

//return: true -> should use replaced path rather than original one
bool rt_mem_check(int n, char** rt_paths, ...){
    char * check_switch = getenv("__priv_switch");
    if(!check_switch){
        return false;
    }

    struct ProcessInfo pinfo;
    char buff[MAX_PATH];
    //could not get process info, let's pass it
    if(!get_process_info(&pinfo)){
        return false;
    }
    va_list args;
    va_start(args, rt_paths);
    const char* function = va_arg(args, const char*);

    char* paths[n];
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
        return rt_mem_con_check(containerid, pinfo.pname, function, n, paths, rt_paths);
    }
    log_fatal("fatal error, can't get container id");
    return false;
}
