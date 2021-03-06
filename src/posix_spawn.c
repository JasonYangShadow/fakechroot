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

#ifdef HAVE_POSIX_SPAWN
#define _GNU_SOURCE
#include <errno.h>
#include <stddef.h>
#include <unistd.h>
#include <spawn.h>
#include <stdlib.h>
#include <fcntl.h>
#include <alloca.h>
#include "strchrnul.h"
#include "libfakechroot.h"
#include "open.h"
#include "setenv.h"
#include "readlink.h"
#include "unionfs.h"
#include <libgen.h>
#include "hmappriv.h"


wrapper(posix_spawn, int, (pid_t* pid, const char * filename, 
        const posix_spawn_file_actions_t* file_actions,
        const posix_spawnattr_t* attrp, char* const argv[],
        char * const envp []))
{
    int status;
    int file;
    int is_base_orig = 0;
    char hashbang[FAKECHROOT_PATH_MAX];
    size_t argv_max = 1024;
    const char **newargv = alloca(argv_max * sizeof (const char *));
    char **newenvp, **ep;
    char *key, *env;
    char tmpkey[1024], *tp;
    char *cmdorig;
    char tmp[FAKECHROOT_PATH_MAX];
    char substfilename[FAKECHROOT_PATH_MAX];
    char newfilename[FAKECHROOT_PATH_MAX];
    char argv0[FAKECHROOT_PATH_MAX];
    char *ptr;
    unsigned int i, j, n, newenvppos;
    unsigned int do_cmd_subst = 0;
    size_t sizeenvp;
    char c;

    char *elfloader = getenv("FAKECHROOT_ELFLOADER");
    char *elfloader_opt_argv0 = getenv("FAKECHROOT_ELFLOADER_OPT_ARGV0");

    if (elfloader && !*elfloader) elfloader = NULL;
    if (elfloader_opt_argv0 && !*elfloader_opt_argv0) elfloader_opt_argv0 = NULL;

    debug("posix_spawn(\"%s\", {\"%s\", ...}, {\"%s\", ...})", filename, argv[0], envp ? envp[0] : "(null)");

    //here we update ld_library_path firstly in order to add latest layers info into ld_library_path e.g rw layer. anothe way is that we directly add all combinations into ld_library_path by default without checking if dir exists
    fakechroot_merge_ld_path(NULL);

    strncpy(argv0, filename, FAKECHROOT_PATH_MAX);

    /* Substitute command only if FAKECHROOT_CMD_ORIG is not set. Unset variable if it is empty. */
    cmdorig = getenv("FAKECHROOT_CMD_ORIG");
    if (cmdorig == NULL)
        do_cmd_subst = fakechroot_try_cmd_subst(getenv("FAKECHROOT_CMD_SUBST"), argv0, substfilename);
    else if (!*cmdorig)
        unsetenv("FAKECHROOT_CMD_ORIG");

    /* Scan envp and check its size */
    sizeenvp = 0;
    if (envp) {
        for (ep = (char **)envp; *ep != NULL; ++ep) {
            sizeenvp++;
        }
    }

    /* Copy envp to newenvp */
    newenvp = malloc( (sizeenvp + preserve_env_list_count + 1) * sizeof (char *) );
    if (newenvp == NULL) {
        __set_errno(ENOMEM);
        return errno;
    }
    newenvppos = 0;

    /* Create new envp */
    newenvp[newenvppos] = malloc(strlen("FAKECHROOT=true") + 1);
    strcpy(newenvp[newenvppos], "FAKECHROOT=true");
    newenvppos++;

    /* Preserve old environment variables if not overwritten by new */
    for (j = 0; j < preserve_env_list_count; j++) {
        key = preserve_env_list[j];
        env = getenv(key);
        if (env != NULL && *env) {
            if (do_cmd_subst && strcmp(key, "FAKECHROOT_BASE") == 0) {
                key = "FAKECHROOT_BASE_ORIG";
                is_base_orig = 1;
            }
            if (envp) {
                for (ep = (char **) envp; *ep != NULL; ++ep) {
                    strncpy(tmpkey, *ep, 1024);
                    tmpkey[1023] = 0;
                    if ((tp = strchr(tmpkey, '=')) != NULL) {
                        *tp = 0;
                        if (strcmp(tmpkey, key) == 0) {
                            goto skip1;
                        }
                    }
                }
            }
            newenvp[newenvppos] = malloc(strlen(key) + strlen(env) + 3);
            strcpy(newenvp[newenvppos], key);
            strcat(newenvp[newenvppos], "=");
            strcat(newenvp[newenvppos], env);
            newenvppos++;
        skip1: ;
        }
    }

    /* Append old envp to new envp */
    if (envp) {
        for (ep = (char **) envp; *ep != NULL; ++ep) {
            strncpy(tmpkey, *ep, 1024);
            tmpkey[1023] = 0;
            if ((tp = strchr(tmpkey, '=')) != NULL) {
                *tp = 0;
                if (strcmp(tmpkey, "FAKECHROOT") == 0 ||
                    (is_base_orig && strcmp(tmpkey, "FAKECHROOT_BASE") == 0))
                {
                    goto skip2;
                }
            }
            newenvp[newenvppos] = *ep;
            newenvppos++;
        skip2: ;
        }
    }

    newenvp[newenvppos] = NULL;

    if (newenvp == NULL) {
        __set_errno(ENOMEM);
        return errno;
    }

    if (do_cmd_subst) {
        newenvp[newenvppos] = malloc(strlen("FAKECHROOT_CMD_ORIG=") + strlen(filename) + 1);
        strcpy(newenvp[newenvppos], "FAKECHROOT_CMD_ORIG=");
        strcat(newenvp[newenvppos], filename);
        newenvppos++;
    }

    newenvp[newenvppos] = NULL;

    /* Exec substituted command */
    if (do_cmd_subst) {
        debug("nextcall(posix_spawn) exec substituted command(\"%s\", {\"%s\", ...}, {\"%s\", ...})", substfilename, argv[0], newenvp[0]);
        char **sp;
        for(sp = (char **)newenvp; *sp != NULL; ++sp){
            if(strncmp(*sp, "LD_LIBRARY_PATH=", strlen("LD_LIBRARY_PATH=")) == 0){
                debug("nextcall(posix_spawn) exec substituted command: %s, LD_LIBRARY_PATH value: %s", substfilename, *sp);
                memset(*sp, '\0', strlen(*sp));
                char *mempid = getenv("MEMCACHED_PID");
                if(mempid && *mempid == '/'){
                    //use memcached_pid path to find .lpmxsys folder, as it contains necessary libraries to start LPMX
                    char *parent = dirname(mempid);
                    char new_ld_path[FAKECHROOT_PATH_MAX];
                    sprintf(new_ld_path, "LD_LIBRARY_PATH=%s", parent);
                    debug("fakechroot ld_library_path is set to %s, as ld_library_path is cleared in subsitituded command", new_ld_path);
                    memcpy(*sp, new_ld_path, strlen(new_ld_path));
                }
            }
        }
        status = nextcall(posix_spawn)(pid, substfilename, file_actions, attrp, (char * const *)argv, newenvp);
        goto error;
    }

    /* Check hashbang */
    //make filename drop const
    char orig_filename[FAKECHROOT_PATH_MAX];
    strcpy(orig_filename, filename);
    //here we need firstly to check if the program being called is mapped or not
    //only if we open the specific swtich, otherwise, it won't be called
    char * exec_switch = getenv("FAKECHROOT_EXEC_SWITCH");
    if(filename && *filename == '/' && exec_switch){
        char replace_path[FAKECHROOT_PATH_MAX];
        char* replace_path_p = replace_path;
        bool exec_ok = rt_mem_exec_map(filename, &replace_path_p);
        if(exec_ok){
            memset(orig_filename, '\0', FAKECHROOT_PATH_MAX);
            strcpy(orig_filename, replace_path_p);
            goto exe_excute;
        }
    }

    //here we have to check if filename is symlink
    expand_chroot_path(filename);
    if(lxstat(filename) && is_file_type(filename, TYPE_LINK)){
        debug("nextcall(posix_spawn) symlink found: %s", filename);
        char link_resolved[FAKECHROOT_PATH_MAX];
        iterResolveSymlink(filename, link_resolved);
        filename = link_resolved;
    }else{
        strcpy(tmp, filename);
        filename = tmp;
    }

    //when the target does not exist, we tried to find if it exists in excluded exe
    if ((file = nextcall(open)(filename, O_RDONLY)) == -1) {
        //get exposed exe
        const char * exe = getenv("FAKECHROOT_EXCLUDE_EXE");
        if(exe){
            char exe_dup[FAKECHROOT_PATH_MAX];
            strcpy(exe_dup, exe);
            char *str_tmp = strtok(exe_dup,":");
            while (str_tmp != NULL){
                if(strcmp(orig_filename, str_tmp) == 0){
                    goto exe_excute;
                }
                str_tmp = strtok(NULL,":");
            }
        }

        debug("could not find executable program with name: %s", orig_filename);

exe_err:
        __set_errno(ENOENT);
        return errno;

exe_excute:
        debug("try to execute exposed program %s from host", orig_filename);
        int menvppos = 0;
        char ** menvp = malloc(sizeof(char *)*newenvppos);
        if(!menvp){
            goto exe_err;
        }
        char ** mep;
        for(mep = newenvp; *mep != NULL; mep++){
            char tvar[1024];
            strncpy(tvar, *mep, 1024);
            tvar[1023] = '\0';
            //clear unecessary env vars
            if(strncmp(tvar,"LD_PRELOAD",strlen("LD_PRELOAD")) == 0){
                continue;
            }
            if(strncmp(tvar,"FAKECHROOT",strlen("FAKECHROOT")) == 0){
                continue;
            }
            if(strncmp(tvar,"Container",strlen("Container")) == 0){
                continue;
            }
            if(strncmp(tvar,"PWD",strlen("PWD")) == 0){
                continue;
            }
            if(strncmp(tvar,"LD_LIBRARY_PATH",strlen("LD_LIBRARY_PATH")) == 0){
                continue;
            }
            if(strncmp(tvar,"SHELL",strlen("SHELL")) == 0){
                continue;
            }
            if(strncmp(tvar,"__",strlen("__")) == 0){
                continue;
            }
            if(strncmp(tvar,"MEMCACHED_PID",strlen("MEMCACHED_PID")) == 0){
                continue;
            }

            debug("env var %s will be added",*mep);
            menvp[menvppos] = *mep;
            menvppos++;
        }
        menvp[menvppos] = NULL;
        status = nextcall(posix_spawn)(pid, orig_filename, file_actions, attrp, argv, menvp);
        free(menvp);
        goto error;
    }

    //read hashbang info
    i = read(file, hashbang, FAKECHROOT_PATH_MAX-2);
    close(file);
    if (i == -1) {
        __set_errno(ENOENT);
        return errno;
    }

    /* No hashbang in argv */
    if (hashbang[0] != '#' || hashbang[1] != '!') {
        if (!elfloader) {
            debug("nextcall(posix_spawn) run without elfloader with no hashbang, filename: %s", filename);
            status = nextcall(posix_spawn)(pid, filename, file_actions, attrp, argv, newenvp);
            goto error;
        }

        /* Run via elfloader */
        for (i = 0, n = (elfloader_opt_argv0 ? 3 : 1); argv[i] != NULL && i < argv_max; ) {
            newargv[n++] = argv[i++];
        }

        newargv[n] = 0;

        n = 0;
        newargv[n++] = elfloader;
        if (elfloader_opt_argv0) {
            newargv[n++] = elfloader_opt_argv0;
            newargv[n++] = argv0;
        }
        newargv[n] = filename;

        int idx = 0;
        while(idx < argv_max && newargv[idx] && *newargv[idx]){
            debug("nextcall(posix_spawn) run via elfloader with no hashbang: argv[%d]: %s", idx, newargv[idx]);
            idx++;
        }
        debug("nextcall(posix_spawn) run via elfloader with no hashbang(\"%s\", {\"%s\", \"%s\", ...}, {\"%s\", ...})", elfloader, newargv[0], newargv[n], newenvp[0]);
        status = nextcall(posix_spawn)(pid, elfloader, file_actions, attrp, (char * const *)newargv, newenvp);
        goto error;
    }

    /* For hashbang we must fix argv[0] */
    hashbang[i] = hashbang[i+1] = 0;
    for (i = j = 2; (hashbang[i] == ' ' || hashbang[i] == '\t') && i < FAKECHROOT_PATH_MAX; i++, j++);
    for (n = 0; i < FAKECHROOT_PATH_MAX; i++) {
        c = hashbang[i];
        if (hashbang[i] == 0 || hashbang[i] == ' ' || hashbang[i] == '\t' || hashbang[i] == '\n') {
            hashbang[i] = 0;
            if (i > j) {
                if (n == 0) {
                    ptr = &hashbang[j];
                    expand_chroot_path(ptr);
                    strcpy(newfilename, ptr);
                }
                newargv[n++] = &hashbang[j];
            }
            j = i + 1;
        }
        if (c == '\n' || c == 0)
            break;
    }

    newargv[n++] = argv0;

    for (i = 1; argv[i] != NULL && i < argv_max; ) {
        newargv[n++] = argv[i++];
    }

    newargv[n] = 0;

    if (!elfloader) {
        debug("nextcall(posix_spawn) run without elfloader with hashbang(\"%s\", {\"%s\", \"%s\", \"%s\", ...}, {\"%s\", ...})", newfilename, newargv[0], newargv[1], newargv[n], newenvp[0]);
        status = nextcall(posix_spawn)(pid, newfilename, file_actions, attrp, (char * const *)newargv, newenvp);
        goto error;
    }

    /* Run via elfloader */
    j = elfloader_opt_argv0 ? 3 : 1;
    if (n >= argv_max - 1) {
        n = argv_max - j - 1;
    }
    newargv[n+j] = 0;
    for (i = n; i >= j; i--) {
        newargv[i] = newargv[i-j];
    }
    n = 0;
    newargv[n++] = elfloader;
    if (elfloader_opt_argv0) {
        newargv[n++] = elfloader_opt_argv0;
        newargv[n++] = argv0;
    }
    newargv[n] = newfilename;

    int idx = 0;
    while(idx < argv_max && newargv[idx] && *newargv[idx]){
        debug("nextcall(posix_spawn) run via elflaoder with hashbang, argv[%d]: %s", idx, newargv[idx]);
        idx++;
    }
    debug("nextcall(posix_spawn) run via elfloader with hashbang(\"%s\", {\"%s\", \"%s\", \"%s\", ...}, {\"%s\", ...})", elfloader, newargv[0], newargv[1], newargv[n], newenvp[0]);
    status = nextcall(posix_spawn)(pid, elfloader, file_actions, attrp, (char * const *)newargv, newenvp);

error:
    free(newenvp);

    return status;
}


//#define _GNU_SOURCE
//#include <errno.h>
//#include <stddef.h>
//#include <unistd.h>
//#include <spawn.h>
//#include <stdlib.h>
//#include <fcntl.h>
//#include <alloca.h>
//#include "strchrnul.h"
//#include "libfakechroot.h"
//#include "open.h"
//#include "setenv.h"
//#include "readlink.h"
//
//
//wrapper(posix_spawn, int, (pid_t* pid, const char * filename, 
//        const posix_spawn_file_actions_t* file_actions,
//        const posix_spawnattr_t* attrp, char* const argv[],
//        char * const envp []))
//{
//    int status;
//    int file;
//    int is_base_orig = 0;
//    char hashbang[FAKECHROOT_PATH_MAX];
//    size_t argv_max = 1024;
//    const char **newargv = alloca(argv_max * sizeof (const char *));
//    char **newenvp, **ep;
//    char *key, *env;
//    char tmpkey[1024], *tp;
//    char *cmdorig;
//    char tmp[FAKECHROOT_PATH_MAX];
//    char substfilename[FAKECHROOT_PATH_MAX];
//    char newfilename[FAKECHROOT_PATH_MAX];
//    char argv0[FAKECHROOT_PATH_MAX];
//    char *ptr;
//    unsigned int i, j, n, newenvppos;
//    unsigned int do_cmd_subst = 0;
//    size_t sizeenvp;
//    char c;
//
//    char *elfloader = getenv("FAKECHROOT_ELFLOADER");
//    char *elfloader_opt_argv0 = getenv("FAKECHROOT_ELFLOADER_OPT_ARGV0");
//
//    if (elfloader && !*elfloader) elfloader = NULL;
//    if (elfloader_opt_argv0 && !*elfloader_opt_argv0) elfloader_opt_argv0 = NULL;
//
//    debug("posix_spawn(\"%s\", {\"%s\", ...}, {\"%s\", ...})", filename, argv[0], envp ? envp[0] : "(null)");
//
//    strncpy(argv0, filename, FAKECHROOT_PATH_MAX);
//
//    /* Substitute command only if FAKECHROOT_CMD_ORIG is not set. Unset variable if it is empty. */
//    cmdorig = getenv("FAKECHROOT_CMD_ORIG");
//    if (cmdorig == NULL)
//        do_cmd_subst = fakechroot_try_cmd_subst(getenv("FAKECHROOT_CMD_SUBST"), argv0, substfilename);
//    else if (!*cmdorig)
//        unsetenv("FAKECHROOT_CMD_ORIG");
//
//    /* Scan envp and check its size */
//    sizeenvp = 0;
//    if (envp) {
//        for (ep = (char **)envp; *ep != NULL; ++ep) {
//            sizeenvp++;
//        }
//    }
//
//    /* Copy envp to newenvp */
//    newenvp = malloc( (sizeenvp + preserve_env_list_count + 1) * sizeof (char *) );
//    if (newenvp == NULL) {
//        __set_errno(ENOMEM);
//        return errno;
//    }
//    newenvppos = 0;
//
//    /* Create new envp */
//    newenvp[newenvppos] = malloc(strlen("FAKECHROOT=true") + 1);
//    strcpy(newenvp[newenvppos], "FAKECHROOT=true");
//    newenvppos++;
//
//    /* Preserve old environment variables if not overwritten by new */
//    for (j = 0; j < preserve_env_list_count; j++) {
//        key = preserve_env_list[j];
//        env = getenv(key);
//        if (env != NULL && *env) {
//            if (do_cmd_subst && strcmp(key, "FAKECHROOT_BASE") == 0) {
//                key = "FAKECHROOT_BASE_ORIG";
//                is_base_orig = 1;
//            }
//            if (envp) {
//                for (ep = (char **) envp; *ep != NULL; ++ep) {
//                    strncpy(tmpkey, *ep, 1024);
//                    tmpkey[1023] = 0;
//                    if ((tp = strchr(tmpkey, '=')) != NULL) {
//                        *tp = 0;
//                        if (strcmp(tmpkey, key) == 0) {
//                            goto skip1;
//                        }
//                    }
//                }
//            }
//            newenvp[newenvppos] = malloc(strlen(key) + strlen(env) + 3);
//            strcpy(newenvp[newenvppos], key);
//            strcat(newenvp[newenvppos], "=");
//            strcat(newenvp[newenvppos], env);
//            newenvppos++;
//        skip1: ;
//        }
//    }
//
//    /* Append old envp to new envp */
//    if (envp) {
//        for (ep = (char **) envp; *ep != NULL; ++ep) {
//            strncpy(tmpkey, *ep, 1024);
//            tmpkey[1023] = 0;
//            if ((tp = strchr(tmpkey, '=')) != NULL) {
//                *tp = 0;
//                if (strcmp(tmpkey, "FAKECHROOT") == 0 ||
//                    (is_base_orig && strcmp(tmpkey, "FAKECHROOT_BASE") == 0))
//                {
//                    goto skip2;
//                }
//            }
//            newenvp[newenvppos] = *ep;
//            newenvppos++;
//        skip2: ;
//        }
//    }
//
//    newenvp[newenvppos] = NULL;
//
//    if (newenvp == NULL) {
//        __set_errno(ENOMEM);
//        return errno;
//    }
//
//    if (do_cmd_subst) {
//        newenvp[newenvppos] = malloc(strlen("FAKECHROOT_CMD_ORIG=") + strlen(filename) + 1);
//        strcpy(newenvp[newenvppos], "FAKECHROOT_CMD_ORIG=");
//        strcat(newenvp[newenvppos], filename);
//        newenvppos++;
//    }
//
//    newenvp[newenvppos] = NULL;
//
//    /* Exec substituted command */
//    if (do_cmd_subst) {
//        debug("nextcall(posix_spawn)(\"%s\", {\"%s\", ...}, {\"%s\", ...})", substfilename, argv[0], newenvp[0]);
//        status = nextcall(posix_spawn)(pid, substfilename, file_actions, attrp, (char * const *)argv, newenvp);
//        goto error;
//    }
//
//    /* Check hashbang */
//    expand_chroot_path(filename);
//    strcpy(tmp, filename);
//    filename = tmp;
//
//    if ((file = nextcall(open)(filename, O_RDONLY)) == -1) {
//        __set_errno(ENOENT);
//        return errno;
//    }
//
//    i = read(file, hashbang, FAKECHROOT_PATH_MAX-2);
//    close(file);
//    if (i == -1) {
//        __set_errno(ENOENT);
//        return errno;
//    }
//
//    /* No hashbang in argv */
//    if (hashbang[0] != '#' || hashbang[1] != '!') {
//        if (!elfloader) {
//            status = nextcall(posix_spawn)(pid, filename, file_actions, attrp, argv, newenvp);
//            goto error;
//        }
//
//        /* Run via elfloader */
//        for (i = 0, n = (elfloader_opt_argv0 ? 3 : 1); argv[i] != NULL && i < argv_max; ) {
//            newargv[n++] = argv[i++];
//        }
//
//        newargv[n] = 0;
//
//        n = 0;
//        newargv[n++] = elfloader;
//        if (elfloader_opt_argv0) {
//            newargv[n++] = elfloader_opt_argv0;
//            newargv[n++] = argv0;
//        }
//        newargv[n] = filename;
//
//        debug("nextcall(posix_spawn)(\"%s\", {\"%s\", \"%s\", ...}, {\"%s\", ...})", elfloader, newargv[0], newargv[n], newenvp[0]);
//        status = nextcall(posix_spawn)(pid, elfloader, file_actions, attrp, (char * const *)newargv, newenvp);
//        goto error;
//    }
//
//    /* For hashbang we must fix argv[0] */
//    hashbang[i] = hashbang[i+1] = 0;
//    for (i = j = 2; (hashbang[i] == ' ' || hashbang[i] == '\t') && i < FAKECHROOT_PATH_MAX; i++, j++);
//    for (n = 0; i < FAKECHROOT_PATH_MAX; i++) {
//        c = hashbang[i];
//        if (hashbang[i] == 0 || hashbang[i] == ' ' || hashbang[i] == '\t' || hashbang[i] == '\n') {
//            hashbang[i] = 0;
//            if (i > j) {
//                if (n == 0) {
//                    ptr = &hashbang[j];
//                    expand_chroot_path(ptr);
//                    strcpy(newfilename, ptr);
//                }
//                newargv[n++] = &hashbang[j];
//            }
//            j = i + 1;
//        }
//        if (c == '\n' || c == 0)
//            break;
//    }
//
//    newargv[n++] = argv0;
//
//    for (i = 1; argv[i] != NULL && i < argv_max; ) {
//        newargv[n++] = argv[i++];
//    }
//
//    newargv[n] = 0;
//
//    if (!elfloader) {
//        status = nextcall(posix_spawn)(pid, newfilename, file_actions, attrp, (char * const *)newargv, newenvp);
//        goto error;
//    }
//
//    /* Run via elfloader */
//    j = elfloader_opt_argv0 ? 3 : 1;
//    if (n >= argv_max - 1) {
//        n = argv_max - j - 1;
//    }
//    newargv[n+j] = 0;
//    for (i = n; i >= j; i--) {
//        newargv[i] = newargv[i-j];
//    }
//    n = 0;
//    newargv[n++] = elfloader;
//    if (elfloader_opt_argv0) {
//        newargv[n++] = elfloader_opt_argv0;
//        newargv[n++] = argv0;
//    }
//    newargv[n] = newfilename;
//    debug("nextcall(posix_spawn)(\"%s\", {\"%s\", \"%s\", \"%s\", ...}, {\"%s\", ...})", elfloader, newargv[0], newargv[1], newargv[n], newenvp[0]);
//    status = nextcall(posix_spawn)(pid, elfloader, file_actions, attrp, (char * const *)newargv, newenvp);
//
//error:
//    free(newenvp);
//
//    return status;
//}

#endif
