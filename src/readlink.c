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

#include <sys/types.h>
#include <stddef.h>
#include "libfakechroot.h"
#include "unionfs.h"
#include "dedotdot.h"
#include <libgen.h>

wrapper(readlink, READLINK_TYPE_RETURN, (const char * path, char * buf, READLINK_TYPE_ARG3(bufsiz)))
{
    int linksize;
    char tmp[MAX_PATH];

    debug("readlink(\"%s\", &buf, %zd)", path, bufsiz);
    if (!strcmp(path, "/etc/malloc.conf")) {
        errno = ENOENT;
        return -1;
    }

    INITIAL_SYS(readlink)

    //processing /proc/self/cwd /proc/self/exe /proc/self/root
    if(strncmp(path,"/proc/self/root", strlen("/proc/self/root")) == 0){
        strcpy(buf,"/");
        linksize = 1;
        return linksize;
    }

    if(strncmp(path, "/proc/self/cwd", strlen("/proc/self/cwd")) == 0){
        if(is_file_type(path, TYPE_LINK)){
            linksize = real_readlink(path, tmp, MAX_PATH -1);
            if(linksize != -1){
                tmp[linksize] = '\0';

                char rel_path[MAX_PATH], layer_path[MAX_PATH];
                int ret = get_relative_path_layer(tmp, rel_path, layer_path);
                if(ret == 0){
                    strncpy(buf, "/", 1);
                    if(strcmp(rel_path, ".") != 0){
                        strncpy(buf+1, rel_path, strlen(rel_path));
                    }
                    linksize = strlen(buf);
                    debug("readlink processing /proc/self/cwd, result: %s, length: %d", buf, linksize);
                    return linksize;
                }else{
                    strcpy(buf, tmp);
                    linksize = strlen(buf);
                    debug("readlink processing /proc/self/cwd, result: %s, length: %d", buf, linksize);
                    return linksize;
                }
            }
            errno = ENOENT;
            debug("/proc/self/cwd could not be successfully read");
            return -1;
        }
    }

    if(strncmp(path, "/proc/self/exe", strlen("/proc/self/exe")) == 0){
        pid_t pid = getpid();
        char pid_path[MAX_PATH];
        sprintf(pid_path, "/proc/%d/cmdline", pid);
        //parse cmdline
        char exec[MAX_PATH];
        if(parse_cmd_line(pid_path, exec)){
            strcpy(buf, exec);
            linksize = strlen(buf);
            debug("readlink processing /proc/self/exe, result: %s, length: %d", buf, linksize);
            return linksize;
        }

        errno = ENOENT;
        debug("/proc/self/exe could not be successfully read");
        return -1;
    }

    expand_chroot_path(path);

    if((linksize = real_readlink(path, tmp, MAX_PATH - 1)) == -1){
        return -1;
    }
    tmp[linksize] = '\0';

    if(*tmp == '/'){
        char resolved[MAX_PATH];
        if(pathExcluded(tmp)){
            strncpy(buf, tmp, linksize);
        }else if(findFileInLayers(tmp,resolved)){
            narrow_path(resolved);
            linksize = strlen(resolved);
            if(linksize > bufsiz){
                linksize = bufsiz;
            }
            strncpy(buf,resolved,linksize);
        }else{
            const char *container_root = getenv("ContainerRoot");
            char resolved_tmp[MAX_PATH];
            if(container_root){
                if(*tmp == '/'){
                    sprintf(resolved_tmp, "%s%s",container_root, tmp);
                }
                narrow_path(resolved_tmp);
                linksize = strlen(resolved_tmp);
                if(linksize > bufsiz){
                    linksize = bufsiz;
                }
                strncpy(buf, resolved_tmp, linksize);
            }else{
                errno = ENOENT;
                return -1;
            }
        }
        buf[linksize] = '\0';
    }else{
        strcpy(buf, tmp);
    }

    debug("readlink resolved: %s", buf);
    return linksize;
}

/**
  wrapper(readlink, READLINK_TYPE_RETURN, (const char * path, char * buf, READLINK_TYPE_ARG3(bufsiz)))
  {
  int linksize;
  char tmp[FAKECHROOT_PATH_MAX], *tmpptr;
  const char *fakechroot_base = getenv("FAKECHROOT_BASE");

  debug("readlink(\"%s\", &buf, %zd)", path, bufsiz);
  if (!strcmp(path, "/etc/malloc.conf")) {
  errno = ENOENT;
  return -1;
  }
  expand_chroot_path(path);

  if ((linksize = nextcall(readlink)(path, tmp, FAKECHROOT_PATH_MAX-1)) == -1) {
  return -1;
  }
  tmp[linksize] = '\0';

  if (fakechroot_base != NULL) {
  tmpptr = strstr(tmp, fakechroot_base);
  if (tmpptr != tmp) {
  tmpptr = tmp;
  }
  else if (tmp[strlen(fakechroot_base)] == '\0') {
  tmpptr = "/";
  linksize = strlen(tmpptr);
  }
  else if (tmp[strlen(fakechroot_base)] == '/') {
  tmpptr = tmp + strlen(fakechroot_base);
  linksize -= strlen(fakechroot_base);
  }
  else {
  tmpptr = tmp;
  }
  if (strlen(tmpptr) > bufsiz) {
  linksize = bufsiz;
  }
  strncpy(buf, tmpptr, linksize);
  }
  else {
  strncpy(buf, tmp, linksize);
  }
  return linksize;
  }
 **/
