/*
   libfakechroot -- fake chroot environment
   Copyright (c) 2003-2015 Piotr Roszatycki <dexter@debian.org>
   Copyright (c) 2007 Mark Eichin <eichin@metacarta.com>
   Copyright (c) 2006, 2007 Alexander Shishkin <virtuoso@slind.org>

   klik2 support -- give direct access to a list of directories
   Copyright (c) 2006, 2007 Lionel Tricon <lionel.tricon@free.fr>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public License as published by the Free Software Foundation; either
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

#define _GNU_SOURCE

#include <dlfcn.h>
#include <pwd.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "getcwd_real.h"
#include "libfakechroot.h"
#include "log.h"
#include "memcached_client.h"
#include "setenv.h"
#include "strchrnul.h"
#include <errno.h>
#include <libgen.h>
#include "unionfs.h"

#define EXCLUDE_LIST_SIZE 100

/* Useful to exclude a list of directories or files */
static char* exclude_list[EXCLUDE_LIST_SIZE];
static int exclude_length[EXCLUDE_LIST_SIZE];
static int list_max = 0;
static int first = 0;

/* List of environment variables to preserve on clearenv() */
char* preserve_env_list[] = {
    "FAKECHROOT_BASE",
    "FAKECHROOT_CMD_SUBST",
    "FAKECHROOT_DEBUG",
    "FAKECHROOT_DETECT",
    "FAKECHROOT_ELFLOADER",
    "FAKECHROOT_ELFLOADER_OPT_ARGV0",
    "FAKECHROOT_EXCLUDE_PATH",
    "FAKECHROOT_LDLIBPATH",
    "FAKECHROOT_VERSION",
    "FAKEROOTKEY",
    "FAKED_MODE",
    "LD_LIBRARY_PATH",
    "LD_PRELOAD"
};
char* ld_env_list[] = {
    "/lib",
    "/lib64",
    "/lib/x86_64-linux-gnu",
    "/usr/lib/x86_64-linux-gnu",
    "/usr/lib",
    "/usr/local/lib",
    "/usr/lib64",
    "/usr/local/lib64"
};
const int preserve_env_list_count = sizeof preserve_env_list / sizeof preserve_env_list[0];
const int ld_env_list_count = sizeof ld_env_list / sizeof ld_env_list[0];

LOCAL int fakechroot_debug(const char* fmt, ...)
{
    int ret;
    char newfmt[2048];

    va_list ap;
    va_start(ap, fmt);

    if (!getenv("FAKECHROOT_DEBUG"))
        return 0;

    sprintf(newfmt, PACKAGE ": %s\n", fmt);

    ret = vfprintf(stderr, newfmt, ap);
    va_end(ap);

    return ret;
}

#include "getcwd.h"

/* Bootstrap the library */
void fakechroot_init(void) CONSTRUCTOR;
void fakechroot_init(void)
{
    char* detect = getenv("FAKECHROOT_DETECT");

    if (detect) {
        /* printf causes coredump on FreeBSD */
        if (write(STDOUT_FILENO, PACKAGE, sizeof(PACKAGE) - 1) && write(STDOUT_FILENO, " ", 1) && write(STDOUT_FILENO, VERSION, sizeof(VERSION) - 1) && write(STDOUT_FILENO, "\n", 1)) { /* -Wunused-result */
        }
        _Exit(atoi(detect));
    }

    debug("fakechroot_init()");
    //debug("FAKECHROOT_BASE=\"%s\"", getenv("FAKECHROOT_BASE"));
    //debug("FAKECHROOT_BASE_ORIG=\"%s\"", getenv("FAKECHROOT_BASE_ORIG"));
    //debug("FAKECHROOT_CMD_ORIG=\"%s\"", getenv("FAKECHROOT_CMD_ORIG"));

    if (!first) {
        char* exclude_path = getenv("FAKECHROOT_EXCLUDE_PATH");

        first = 1;

        /* We get a list of directories or files */
        if (exclude_path) {
            int i;
            for (i = 0; list_max < EXCLUDE_LIST_SIZE;) {
                int j;
                for (j = i; exclude_path[j] != ':' && exclude_path[j] != '\0'; j++)
                    ;
                exclude_list[list_max] = malloc(j - i + 2);
                memset(exclude_list[list_max], '\0', j - i + 2);
                strncpy(exclude_list[list_max], &(exclude_path[i]), j - i);
                exclude_length[list_max] = strlen(exclude_list[list_max]);
                list_max++;
                if (exclude_path[j] != ':')
                    break;
                i = j + 1;
            }
        }

        __setenv("FAKECHROOT", "true", 1);
        __setenv("FAKECHROOT_VERSION", FAKECHROOT, 1);
    }

    //process ld_library_path
    fakechroot_merge_ld_path();
}

/* Lazily load function */
LOCAL fakechroot_wrapperfn_t fakechroot_loadfunc(struct fakechroot_wrapper* w)
{
    char* msg;
    if (!(w->nextfunc = dlsym(RTLD_NEXT, w->name))) {
        ;
        msg = dlerror();
        debug("fakechroot_loadfunc %s",w->name);
        fprintf(stderr, "%s: %s: %s\n", PACKAGE, w->name, msg != NULL ? msg : "unresolved symbol");
        exit(EXIT_FAILURE);
    }
    return w->nextfunc;
}

/* Check if path is on exclude list */
LOCAL int fakechroot_localdir(const char* p_path)
{
    char* v_path = (char*)p_path;
    char cwd_path[FAKECHROOT_PATH_MAX];

    if (!p_path)
        return 0;

    if (!first)
        fakechroot_init();

    /* We need to expand relative paths */
    if (p_path[0] != '/') {
        getcwd_real(cwd_path, FAKECHROOT_PATH_MAX);
        v_path = cwd_path;
        //narrow_chroot_path(v_path);
    }

    /* We try to find if we need direct access to a file */
    {
        const size_t len = strlen(v_path);
        int i;

        for (i = 0; i < list_max; i++) {
            if (exclude_length[i] > len || v_path[exclude_length[i] - 1] != (exclude_list[i])[exclude_length[i] - 1] || strncmp(exclude_list[i], v_path, exclude_length[i]) != 0)
                continue;
            if (exclude_length[i] == len || v_path[exclude_length[i]] == '/')
                return 1;
        }
    }

    return 0;
}

/*
 * Parse the FAKECHROOT_CMD_SUBST environment variable (the first
 * parameter) and if there is a match with filename, return the
 * substitution in cmd_subst.  Returns non-zero if there was a match.
 *
 * FAKECHROOT_CMD_SUBST=cmd=subst:cmd=subst:...
 */
LOCAL int fakechroot_try_cmd_subst(char* env, const char* filename, char* cmd_subst)
{
    int len, len2;
    char* p;

    if (env == NULL || filename == NULL)
        return 0;

    /* Remove trailing dot from filename */
    if (filename[0] == '.' && filename[1] == '/')
        filename++;
    len = strlen(filename);

    do {
        p = strchrnul(env, ':');

        if (strncmp(env, filename, len) == 0 && env[len] == '=') {
            len2 = p - &env[len + 1];
            if (len2 >= FAKECHROOT_PATH_MAX)
                len2 = FAKECHROOT_PATH_MAX - 1;
            strncpy(cmd_subst, &env[len + 1], len2);
            cmd_subst[len2] = '\0';
            return 1;
        }

        env = p;
    } while (*env++ != '\0');

    return 0;
}

/**
 * Find and assemble ld_library_path items. return zero is it is successful and return -1 if any errors occur
 * ret size should be LD_MAX_SIZE
 */
LOCAL int fakechroot_assemble_ld_path(char* ret){
    char *layers = getenv("ContainerLayers");
    if(!layers || (layers && *layers == '\0')){
        debug("could not get ContainerLayers env var, return");
        return -1;
    }
    char *base_root = getenv("ContainerBasePath");
    if(!base_root || (base_root && *base_root == '\0')){
        debug("could not get ContainerBasePath env var, return");
        return -1;
    }
    char *con_root = getenv("ContainerRoot");
    if(!con_root || (con_root && *con_root == '\0')){
        debug("could not get ContainerRoot env var, return");
        return -1;
    }
    if(!ret){
        debug("ret is null,return");
        return -1;
    }
    //memset ret
    memset(ret, '\0', LD_MAX_SIZE);

    char rest[FAKECHROOT_PATH_MAX];
    strcpy(rest, layers);
    char *rest_p = rest;
    char *token = NULL;
    while((token = strtok_r(rest_p,":",&rest_p))){
        //token represents current split
        //if rw layer
        if(strcmp(token, "rw") == 0){
            //start check each item in ld_env_list
            for(int i = 0; i<ld_env_list_count; i++){
                char tmp_path[FAKECHROOT_PATH_MAX];
                //initialize
                memset(tmp_path, '\0', FAKECHROOT_PATH_MAX);
                sprintf(tmp_path, "%s%s", con_root, ld_env_list[i]);
                if(xstat(tmp_path)){
                    //add : to the end of path
                    memcpy(tmp_path + strlen(tmp_path), ":", 1);
                    memcpy(ret + strlen(ret), tmp_path, strlen(tmp_path));
                }
            }
        }else{
            //other layers
            for(int i = 0; i<ld_env_list_count; i++){
                char tmp_path[FAKECHROOT_PATH_MAX];
                memset(tmp_path, '\0', FAKECHROOT_PATH_MAX);
                sprintf(tmp_path, "%s/%s%s", base_root, token, ld_env_list[i]);
                if(xstat(tmp_path)){
                    //add : to the end of the path
                    memcpy(tmp_path + strlen(tmp_path), ":", 1);
                    memcpy(ret + strlen(ret), tmp_path, strlen(tmp_path));
                }
            }
        }
    }
    //debug("fakechroot_assemble_ld_path assemble ld_path: %s", ret);
    return 0;
}

/**
 * merge ld_path based on current generated ld_path
 * target size should be LD_MAX_SIZE
 */

//each time when current LD_LIBRARY_PATH does not include necessary generated necessary ld paths, we have to add them to it in order for correct searching
LOCAL int fakechroot_merge_ld_path(){
    char* ld_path = getenv("LD_LIBRARY_PATH");
    char* ld_skip = getenv("LD_LIBRARY_PATH_SKIP");
    //here LD_LIBRARY_PATH_SKIP will be set if we exec substitude command
    if(!ld_path && ld_skip && strcmp(ld_skip,"1") == 0){
        return 0;
    }

    //here we declare a ld_path that could contain all info
    char ld_ret[LD_MAX_SIZE];
    memset(ld_ret, '\0', LD_MAX_SIZE);

    //generate necessary ld_paths
    char gen_ld_path[LD_MAX_SIZE];
    char* gen_ld_path_p = gen_ld_path;
    int ret = fakechroot_assemble_ld_path(gen_ld_path_p);
    if(ret != 0){
        debug("fakechroot could not assemble ld path");
        return -1;
    }

    //current ld_path
    size_t num;
    char** ld_splits = splitStrs(ld_path, &num, ":");
    //here we start expanding ld_path if necessary in order to make sure all items locate inside container
    if(num > 0 && ld_splits){
        expand_ld_path(ld_splits, num);
        for(size_t idx = 0; idx<num; idx++){
            char ld_item[FAKECHROOT_PATH_MAX];
            sprintf(ld_item, "%s:", ld_splits[idx]);
            //initialize ld_ret with expanded value
            memcpy(ld_ret + strlen(ld_ret), ld_item, strlen(ld_item));
        }
    }
    //the algorithm acts like this:
    //split generated ld path and iterately check if substr exists in given target, if not, append it to the end of target
    //here we assume that target contains '\0' as the ending terminator.
    char* token = NULL;
    while((token = strtok_r(gen_ld_path_p,":",&gen_ld_path_p))){
        bool isFound = false;
        for(size_t i =0; i<num; i++){
            if(strcmp(token, ld_splits[i]) == 0){
                isFound = true;
                break;
            }
        }
        if(isFound){
            continue;
        }else{
            char ld_item[FAKECHROOT_PATH_MAX];
            //the last char is not ':'
            if(strlen(ld_ret) > 0 && ld_ret[strlen(ld_ret)-1] != ':'){
                ld_ret[strlen(ld_ret)] = ':';
            }
            //add other missing items
            sprintf(ld_item, "%s:", token);
            memcpy(ld_ret+ strlen(ld_ret), ld_item, strlen(ld_item));
        }
    }

    if(strlen(ld_ret) > LD_MAX_SIZE - 2){
        debug("assembled ld_path is longer than LD_MAX_SIZE, will cause error, ld_path length: %d, LD_MAX_SIZE: %d", strlen(ld_path), LD_MAX_SIZE);
        //truncate
        ld_ret[LD_MAX_SIZE-1] = '\0';
    }

    for(int i = 0; i<num; i++){
        if(ld_splits[i]){
            free(ld_splits[i]);
        }
    }
    if(ld_splits){
        free(ld_splits);
    }

    if(strlen(ld_ret) > strlen(ld_path)){
        //remove last ':'
        //memset(ld_ret+strlen(ld_ret)-1,'\0',1);
        __setenv("LD_LIBRARY_PATH", ld_ret, 1);
    }
    return 0;
}

/**
 * this function is used for expanding str if not inside container, but keeping original value if fails expanding.
 * we will not copy the orignial value, but directly memcpy and extend it
 */

LOCAL int expand_ld_path(char **values, size_t num){
    if(!values || num <= 0){
        log_debug("expand_str fails because of null value");
        return -1;
    }

    //we only expand if the value is absolute path
    for(size_t idx = 0; idx<num; idx++){
        if(values[idx] && *(values[idx]) == '/' && !is_inside_container(values[idx])){
            char val_tmp[FAKECHROOT_PATH_MAX];
            strcpy(val_tmp, values[idx]);
            char *val_tmp_p = val_tmp;
            expand_chroot_path(val_tmp_p);
            if(!xstat(val_tmp_p) && !lxstat(val_tmp_p)){
                //restore
                log_debug("expand_str restore because target does not exists: val: %s", values[idx]);
                continue;
            }
            if(strcmp(values[idx], val_tmp_p) == 0){
                log_debug("expand_str returns because target is the same to original value: val: %s", values[idx]);
                continue;
            }
            strcpy(values[idx], val_tmp_p);
            log_debug("expand_str successfully expand ld_library_path: val: %s", values[idx]);
        }
    }
    return 0;
}

