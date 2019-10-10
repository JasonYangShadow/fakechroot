#include "unionfs.h"
#include "crypt.h"
#include "log.h"
#include "memcached_client.h"
#include <errno.h>
#include <libgen.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "dedotdot.h"
#include <fcntl.h>
#include "util.h"

DIR* getDirents(const char* name, struct dirent_obj** darr, size_t* num)
{
    INITIAL_SYS(opendir)
        INITIAL_SYS(readdir)

        DIR* dirp = real_opendir(name);
    struct dirent* entry = NULL;
    struct dirent_obj* curr = NULL;
    *darr = NULL;
    *num = 0;
    while (entry = real_readdir(dirp)) {
        if ((strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)) {
            continue;
        }
        struct dirent_obj* tmp = (struct dirent_obj*)debug_malloc(sizeof(struct dirent_obj));
        tmp->dp = (struct dirent* )debug_malloc(sizeof(struct dirent));
        memcpy(tmp->dp, entry, sizeof(struct dirent));
        //dirent 64
        tmp->dp64 = (struct dirent64*)debug_malloc(sizeof(struct dirent64));
        tmp->dp64->d_ino = entry->d_ino;
        tmp->dp64->d_off = entry->d_off;
        tmp->dp64->d_reclen = entry->d_reclen;
        tmp->dp64->d_type = entry->d_type;
        strcpy(tmp->dp64->d_name, entry->d_name);

        tmp->next = NULL;
        if(name[strlen(name)-1] == '/'){
            sprintf(tmp->abs_path,"%s%s",name,entry->d_name);
        }else{
            sprintf(tmp->abs_path,"%s/%s",name,entry->d_name);
        }
        dedotdot(tmp->abs_path);
        sprintf(tmp->d_name,"%s",entry->d_name);
        if (*darr == NULL) {
            *darr = curr = tmp;
        } else {
            curr->next = tmp;
            curr = tmp;
        }
        (*num)++;
    }

    rewinddir(dirp);
    return dirp;
}

void getDirentsNoRet(const char* name, struct dirent_obj** darr, size_t *num){
    INITIAL_SYS(closedir)
        DIR* dirp = getDirents(name, darr, num);
    real_closedir(dirp);
}

DIR * getDirentsWh(const char* name, struct dirent_obj** darr, size_t *num, struct dirent_obj** wh_darr, size_t *wh_num){
    INITIAL_SYS(opendir)
        INITIAL_SYS(readdir)

        DIR* dirp = real_opendir(name);
    struct dirent* entry = NULL;
    struct dirent_obj* curr = NULL;
    struct dirent_obj* wh_curr = NULL;
    *darr = *wh_darr = NULL;
    *num = *wh_num = 0;
    while (entry = real_readdir(dirp)) {
        //if is in layers root location, do not add (".","..") as root location does not have ./..
        if ((strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)) {
            continue;
        }
        struct dirent_obj* tmp = (struct dirent_obj*)debug_malloc(sizeof(struct dirent_obj));
        tmp->dp = (struct dirent* )debug_malloc(sizeof(struct dirent));
        memcpy(tmp->dp, entry, sizeof(struct dirent));
        //dirent 64
        tmp->dp64 = (struct dirent64*)debug_malloc(sizeof(struct dirent64));
        tmp->dp64->d_ino = entry->d_ino;
        tmp->dp64->d_off = entry->d_off;
        tmp->dp64->d_reclen = entry->d_reclen;
        tmp->dp64->d_type = entry->d_type;
        strcpy(tmp->dp64->d_name, entry->d_name);

        tmp->next = NULL;
        if(name[strlen(name)-1] == '/'){
            sprintf(tmp->abs_path,"%s%s",name,entry->d_name);
        }else{
            sprintf(tmp->abs_path,"%s/%s",name,entry->d_name);
        }
        dedotdot(tmp->abs_path);
        sprintf(tmp->d_name,"%s",entry->d_name);

        //if .wh file
        char trans[MAX_PATH];
        if(transWh2path(tmp->d_name, PREFIX_WH, trans)){
            //modifyu abs_path
            sprintf(tmp->d_name,"%s", trans);
            if (*wh_darr == NULL) {
                *wh_darr = wh_curr = tmp;
            } else {
                wh_curr->next = tmp;
                wh_curr = tmp;
            }
            (*wh_num)++;
        }else{
            if (*darr == NULL) {
                *darr = curr = tmp;
            } else {
                curr->next = tmp;
                curr = tmp;
            }
            (*num)++;
        }

    }//while ends

    rewinddir(dirp);
    return dirp;

}
void getDirentsWhNoRet(const char* name, struct dirent_obj** darr, size_t *num, struct dirent_obj** wh_darr, size_t *wh_num){
    INITIAL_SYS(closedir)
        DIR* dirp = getDirentsWh(name, darr, num, wh_darr, wh_num);
    real_closedir(dirp);
}

void getDirentsOnlyNames(const char* name, char ***names,size_t *num){
    INITIAL_SYS(opendir)
        INITIAL_SYS(readdir)
        INITIAL_SYS(closedir)

        DIR* dirp = real_opendir(name);
    struct dirent* entry = NULL;
    *names = (char **)debug_malloc(sizeof(char *)*MAX_VALUE_SIZE);
    *num = 0;
    while(entry = real_readdir(dirp)){
        if ((strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)) {
            continue;
        }
        (*names)[*num] = strdup(entry->d_name);
        (*num)++;
    }
    real_closedir(dirp);
}
/**
 * code is modified as all layers except rw layers are relocated
 */
char ** getLayerPaths(size_t *num){
    const char * dockerbase = getenv("DockerBase");
    const char* croot = getenv("ContainerRoot");
    if(!croot){
        log_fatal("can't get container root info");
        return NULL;
    }
    if(dockerbase && strcmp(dockerbase,"TRUE") == 0){
        const char * clayers = getenv("ContainerLayers");
        //const char * base = getenv("ContainerBasePath");
        if (!clayers) {
            log_fatal("can't get container layers info, please set env variable 'ContainerLayers'");
            return NULL;
        }
        char ccroot[MAX_PATH];
        strcpy(ccroot, croot);
        dirname(ccroot);
        *num = 0;
        char *str_tmp;
        char **paths = (char**)debug_malloc(sizeof(char*) * MAX_LAYERS);
        char cclayers[MAX_PATH];
        strcpy(cclayers, clayers);
        str_tmp = strtok(cclayers,":");
        while (str_tmp != NULL){
            paths[*num] = (char *)debug_malloc(MAX_PATH);
            sprintf(paths[*num], "%s/%s", ccroot, str_tmp);
            str_tmp = strtok(NULL,":");
            (*num) ++;
        }
        return paths;
    }
    *num = 0;
    return NULL;
}

void filterMemDirents(const char* name, struct dirent_obj* darr, size_t num)
{
    struct dirent_obj* curr = darr;
    char** keys = (char**)debug_malloc(sizeof(char*) * num);
    size_t* key_lengths = (size_t*)debug_malloc(sizeof(size_t) * num);
    for (int i = 0; i < num; i++) {
        keys[i] = (char*)debug_malloc(sizeof(char) * MAX_PATH);
        strcpy(keys[i], curr->d_name);
        key_lengths[i] = strlen(curr->d_name);
        curr = curr->next;
    }
    char** values = (char**)debug_malloc(sizeof(char*) * num);
    for (int i = 0; i < num; i++) {
        values[i] = (char*)debug_malloc(sizeof(char) * MAX_PATH);
    }
    getMultipleValues((const char **)keys, key_lengths, num, values);
    //delete item in chains at specific pos
    for (int i = 0; i < num; i++) {
        if (values[i] != NULL && strlen(values[i]) != 0) {
            log_debug("delete dirent according to query on memcached value: %s, name is: %s", values[i], keys[i]);
            deleteItemInChain(&darr, i);
        }
    }
}

void deleteItemInChain(struct dirent_obj** darr, size_t num)
{
    size_t i = 0;
    struct dirent_obj *curr=NULL, *prew = *darr;
    if (*darr == NULL) {
        return;
    }
    //delete header
    if (num == 0) {
        curr = curr->next;
        debug_free(prew->dp);
        debug_free(prew->dp64);
        debug_free(prew);
        *darr = curr;
        return;
    }
    for (int i = 0; i < num; i++) {
        if (curr == NULL) {
            break;
        }
        prew = curr;
        curr = curr->next;
    }
    if (curr) {
        prew->next = curr->next;
        debug_free(curr->dp);
        debug_free(curr->dp64);
        debug_free(curr);
    }
}

//delete item pointed by curr pointer, make it point to the next item
void deleteItemInChainByPointer(struct dirent_obj** darr, struct dirent_obj** curr)
{
    if (*darr == NULL || *curr == NULL) {
        return;
    }
    if (*darr == *curr) {
        *curr = (*curr)->next;
        debug_free((*darr)->dp);
        debug_free((*darr)->dp64);
        debug_free(*darr);
        *darr = *curr;
        return;
    }
    struct dirent_obj *p1, *p2;
    p1 = p2 = *darr;
    while (p2) {
        if (p2 == *curr) {
            p1->next = (*curr)->next;
            debug_free((*curr)->dp);
            debug_free((*curr)->dp64);
            debug_free(*curr);
            *curr = p1->next;
            return;
        }
        p1 = p2;
        p2 = p2->next;
    }
}

void addItemToHead(struct dirent_obj** darr, struct dirent* item)
{
    if (*darr == NULL || item == NULL) {
        return;
    }
    struct dirent_obj* curr = (struct dirent_obj*)debug_malloc(sizeof(struct dirent_obj));
    curr->dp = item;
    curr->next = *darr;
    *darr = curr;
}

void addItemToHeadV64(struct dirent_obj** darr, struct dirent64* item)
{
    if (*darr == NULL || item == NULL) {
        return;
    }
    struct dirent_obj* curr = (struct dirent_obj*)debug_malloc(sizeof(struct dirent_obj));
    curr->dp64 = item;
    curr->next = *darr;
    *darr = curr;
}

struct dirent* popItemFromHead(struct dirent_obj** darr)
{
    if (*darr == NULL) {
        return NULL;
    }
    struct dirent_obj* curr = *darr;
    if (curr != NULL) {
        *darr = curr->next;
        struct dirent* ret = curr->dp;
        debug_free(curr->dp64);
        debug_free(curr);
        return ret;
    }
    return NULL;
}

struct dirent64* popItemFromHeadV64(struct dirent_obj** darr)
{
    if (*darr == NULL) {
        return NULL;
    }
    struct dirent_obj* curr = *darr;
    if (curr != NULL) {
        *darr = curr->next;
        struct dirent64* ret = curr->dp64;
        debug_free(curr->dp);
        debug_free(curr);
        return ret;
    }
    return NULL;
}

void clearItems(struct dirent_obj** darr)
{
    if (*darr == NULL) {
        return;
    }
    while (*darr != NULL) {
        struct dirent_obj* next = (*darr)->next;
        debug_free((*darr)->dp);
        debug_free((*darr)->dp64);
        debug_free(*darr);
        *darr = next;
    }
    darr = NULL;
}

bool parse_cmd_line(const char *cmdline, char *execute){
    if(is_file_type(cmdline, TYPE_FILE)){
        INITIAL_SYS(fopen)
            FILE *f = real_fopen(cmdline, "r");
        if(f){
            int c;
            //skip first \0
            while((c = fgetc(f)) != '\0');
            int idx = 0;
            char tmp[MAX_PATH];
            while ((c = fgetc(f)) != '\0')
            {
                tmp[idx++] = c;
            }
            tmp[idx] = '\0';

            //get execuate
            char rel_path[MAX_PATH], layer_path[MAX_PATH];
            int ret = get_relative_path_layer(tmp, rel_path, layer_path);
            if(ret == 0){
                execute[0] = '/';
                sprintf(execute, "/%s", rel_path);
            }else{
                strcpy(execute, tmp);
            }

            fclose(f);
            return true;
        }
    }
    return false;
}
/**
  char* struct2hash(void* pointer, enum hash_type type)
  {
  if (!pointer) {
  return NULL;
  }
  unsigned char ubytes[16];
  char salt[20];
  const char* const salts = "./0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

//retrieve 16 unpredicable bytes form the os
if (getentropy(ubytes, 16)) {
log_fatal("can't retrieve random bytes from os");
return NULL;
}
salt[0] = '$';
if (type == md5) {
salt[1] = '1';
} else if (type == sha256) {
salt[1] = '5';
} else {
log_fatal("hash type error, it should be either 'md5' or 'sha256'");
return NULL;
}
salt[2] = '$';
for (int i = 0; i < 16; i++) {
salt[3 + i] = salts[ubytes[i] & 0x3f];
}
salt[19] = '\0';

char* hash = crypt((char*)pointer, salt);
if (!hash || hash[0] == '*') {
log_fatal("can't hash the struct");
return NULL;
}
if (type == md5) {
log_debug("md5 %s", hash);
char* value = (char*)debug_malloc(sizeof(char) * 23);
strcpy(value, hash + 12);
return value;
} else if (type == sha256) {
log_debug("sha256 %s", hash);
char* value = (char*)debug_malloc(sizeof(char) * 44);
strcpy(value, hash + 20);
return value;
} else {
return NULL;
}
return NULL;
}
 **/

int get_relative_path(const char* path, char* rel_path)
{
    const char* container_path = getenv("ContainerRoot");
    if (container_path) {
        if (strncmp(container_path, path, strlen(container_path)) != 0) {
            strcpy(rel_path, path);
            return -1;
        }
        if (strlen(path) == strlen(container_path)) {
            strcpy(rel_path, "");
        } else {
            strncpy(rel_path, path + strlen(container_path), strlen(path) - strlen(container_path));
            if (rel_path[strlen(rel_path) - 1] == '/') {
                rel_path[strlen(rel_path) - 1] = '\0';
            }
        }
        return 0;
    } else {
        return -1;
    }
}

int get_relative_path_layer(const char *path, char * rel_path, char * layer_path){
    size_t num;
    char ** layers = getLayerPaths(&num);
    if(layers && num>0){
        for(size_t i = 0; i<num; i++){
            char * ret = strstr(path, layers[i]);
            if(ret){
                if(strlen(path) > strlen(layers[i])){
                    strcpy(rel_path, path + strlen(layers[i]) + 1);
                }else{
                    strcpy(rel_path,".");
                }
                strcpy(layer_path, layers[i]);
                if(layers){
                    for(size_t j =0;j<num;j++){
                        debug_free(layers[j]);
                    }
                    debug_free(layers);
                }
                return 0;
            }

            //check other layers rather than rw one
            char * layer_name = basename(layers[i]);
            if(strcmp(layer_name, "rw") != 0){
                char newlayer[MAX_PATH];
                const char * base_path = getenv("ContainerBasePath");
                if(!base_path){
                    log_fatal("can't get variable 'ContainerBasePath'");
                    return -1;
                }
                sprintf(newlayer, "%s/%s", base_path, layer_name);

                char * ret = strstr(path, newlayer);
                if(ret){
                    if(strlen(path) > strlen(newlayer)){
                        strcpy(rel_path, path + strlen(newlayer) + 1);
                    }else{
                        strcpy(rel_path,".");
                    }
                    strcpy(layer_path, newlayer);
                    if(layers){
                        for(size_t j =0;j<num;j++){
                            debug_free(layers[j]);
                        }
                        debug_free(layers);
                    }
                    return 0;

                }
            }
        }
    }

    if(layers){
        for(size_t j =0;j<num;j++){
            debug_free(layers[j]);
        }
        debug_free(layers);
    }
    return -1;
}

int get_abs_path(const char* path, char* abs_path, bool force)
{
    const char* container_path = getenv("ContainerRoot");
    if (container_path) {
        if (force) {
            if (*path == '/') {
                sprintf(abs_path, "%s%s", container_path, path);
            } else {
                sprintf(abs_path, "%s/%s", container_path, path);
            }
        } else {
            if (*path == '/') {
                strcpy(abs_path, path);
            } else {
                sprintf(abs_path, "%s/%s", container_path, path);
            }
        }
        return 0;
    } else {
        log_fatal("can't get variable 'ContainerRoot'");
        return -1;
    }
}

int get_abs_path_base(const char *base, const char *path, char * abs_path, bool force){
    if (base) {
        if (force) {
            if (*path == '/') {
                sprintf(abs_path, "%s%s", base, path);
            } else {
                sprintf(abs_path, "%s/%s", base, path);
            }
        } else {
            if (*path == '/') {
                strcpy(abs_path, path);
            } else {
                sprintf(abs_path, "%s/%s", base, path);
            }
        }
        return 0;
    } else {
        log_fatal("base should not be NULL");
        return -1;
    }
}

int narrow_path(const char *path, char *resolved){
    if(path && *path == '/'){
        char rel_path[MAX_PATH];
        char layer_path[MAX_PATH];
        int ret = get_relative_path_layer(path, rel_path, layer_path);
        if(ret == 0){
            if(strcmp(rel_path, ".") == 0){
                strcpy(resolved,"/");
            }else{
                sprintf(resolved,"/%s",rel_path);
            }
            return 0;
        }
        strcpy(resolved, path);
        return 0;
    }
    strcpy(resolved, path);
    return -1;
}

int get_relative_path_base(const char *base, const char *path, char * rel_path){
    if (base) {
        if (strncmp(base, path, strlen(base)) != 0) {
            strcpy(rel_path, path);
            return -1;
        }
        if (strlen(path) == strlen(base)) {
            strcpy(rel_path, "");
        } else {
            strncpy(rel_path, path + strlen(base), strlen(path) - strlen(base));
            if (rel_path[strlen(rel_path) - 1] == '/') {
                rel_path[strlen(rel_path) - 1] = '\0';
            }
        }
        return 0;
    } else {
        return -1;
    }
}

int append_to_diff(const char* content)
{
    const char* docker = getenv("DockerBase");
    if (strcmp(docker, "TRUE") == 0) {
        const char* diff_path = getenv("ContainerDiff");
        if (diff_path) {
            char target_file[MAX_PATH];
            sprintf(target_file, "%s/.info", diff_path);
            FILE* pfile = fopen(target_file, "a");
            if (pfile == NULL) {
                log_fatal("can't open file %s", target_file);
                return -1;
            }
            fprintf(pfile, "%s\n", content);
            fclose(pfile);
            return 0;
        } else {
            log_debug("unable to get ContainerDiff variable while in docker_base mode");
            return -1;
        }
    }
    return 0;
}

//TYPE_LINK here is symlink
bool is_file_type(const char* path, enum filetype t)
{
    INITIAL_SYS(__lxstat)
    struct stat path_stat;
    int ret = real___lxstat(1,path, &path_stat);
    if (ret == 0) {
        switch (t) {
            case TYPE_FILE:
                return S_ISREG(path_stat.st_mode);
            case TYPE_DIR:
                return S_ISDIR(path_stat.st_mode);
            case TYPE_LINK:
                return S_ISLNK(path_stat.st_mode);
            case TYPE_SOCK:
                return S_ISSOCK(path_stat.st_mode);
            default:
                log_fatal("is_file_type processes file: %s with unrecognized type", path);
                break;
        }
        return false;
    } else {
        log_fatal("is_file_type processes file: %s with error: %s", path, strerror(errno));
        return false;
    }
}

bool transWh2path(const char* name, const char* pre, char* tname)
{
    size_t lenname = strlen(name);
    size_t lenpre = strlen(pre);
    bool b_contain = strncmp(pre, name, lenpre) == 0;
    if (b_contain) {
        strcpy(tname, name + lenpre + 1);
    }
    return b_contain;
}

bool findFileInLayers(const char *file,char *resolved){
    size_t num;
    char ** layers = getLayerPaths(&num);
    if(num > 0){
        char rel_path[MAX_PATH];
        char layer_path[MAX_PATH];
        if(*file == '/'){
            int ret = get_relative_path_layer(file, rel_path, layer_path);
            if(ret == 0){
                for(size_t i = 0; i<num; i++){
                    char tmp[MAX_PATH];
                    if(strcmp(rel_path, ".") == 0){
                        strcpy(tmp, layers[i]);
                    }else{
                        sprintf(tmp,"%s/%s", layers[i],rel_path);
                    }
                    if(getParentWh(tmp)){
                        strcpy(resolved,file);
                        goto frelease;
                    }
                    if(xstat(tmp)){
                        strcpy(resolved,tmp);
                        goto trelease;
                    }
                    if(strcmp(layer_path, layers[i]) == 0){
                        strcpy(resolved,file);
                        goto frelease;
                    }
                }
            }else{
                for(size_t i = 0; i<num; i++){
                    char tmp[MAX_PATH];
                    sprintf(tmp,"%s%s",layers[i],file);
                    if(getParentWh(tmp)){
                        strcpy(resolved,file);
                        goto frelease;
                    }
                    if(xstat(tmp)){
                        strcpy(resolved,tmp);
                        goto trelease;
                    }
                }
            }
        }else{
            for(size_t i = 0; i<num; i++){
                char tmp[MAX_PATH];
                sprintf(tmp,"%s/%s",layers[i],file);
                if(getParentWh(tmp)){
                    strcpy(resolved,file);
                    goto frelease;
                }
                if(xstat(tmp)){
                    strcpy(resolved,tmp);
                    goto trelease;
                }
            }
        }
    }
    strcpy(resolved,file);
    goto frelease;

frelease:
    if(layers){
        for(int i = 0; i < num; i++){
            debug_free(layers[i]);
        }
        debug_free(layers);
    }
    return false;

trelease:
    if(layers){
        for(int i = 0; i < num; i++){
            debug_free(layers[i]);
        }
        debug_free(layers);
    }
    return true;
}

bool findFileInLayersSkip(const char *file, char *resolved, size_t skip){
    size_t num;
    char ** layers = getLayerPaths(&num);
    if(num > 0 && num > skip && skip > 0){
        char rel_path[MAX_PATH];
        char layer_path[MAX_PATH];
        if(*file == '/'){
            int ret = get_relative_path_layer(file, rel_path, layer_path);
            if(ret == 0){
                for(size_t i = skip; i<num; i++){
                    char tmp[MAX_PATH];
                    if (strcmp(rel_path,".") == 0) {
                        strcpy(tmp, layers[i]);
                    } else {
                        sprintf(tmp,"%s/%s", layers[i],rel_path);
                    }
                    if(getParentWh(tmp)){
                        strcpy(resolved,file);
                        goto frelease;
                    }
                    if(xstat(tmp)){
                        strcpy(resolved,tmp);
                        goto trelease;
                    }
                    if(strcmp(layer_path, layers[i]) == 0){
                        strcpy(resolved,file);
                        goto frelease;
                    }
                }
            }else{
                for(size_t i = skip; i<num; i++){
                    char tmp[MAX_PATH];
                    sprintf(tmp,"%s%s",layers[i],file);
                    if(getParentWh(tmp)){
                        strcpy(resolved,file);
                        goto frelease;
                    }
                    if(xstat(tmp)){
                        strcpy(resolved,tmp);
                        goto trelease;
                    }
                }
            }
        }else{
            for(size_t i = skip; i<num; i++){
                char tmp[MAX_PATH];
                sprintf(tmp,"%s/%s",layers[i],file);
                if(getParentWh(tmp)){
                    strcpy(resolved,file);
                    goto frelease;
                }
                if(xstat(tmp)){
                    strcpy(resolved,tmp);
                    goto trelease;
                }
            }
        }
    }


    strcpy(resolved,file);
    goto frelease;

frelease:
    if(layers){
        for(int i = 0; i < num; i++){
            debug_free(layers[i]);
        }
        debug_free(layers);
    }
    return false;

trelease:
    if(layers){
        for(int i = 0; i < num; i++){
            debug_free(layers[i]);
        }
        debug_free(layers);
    }
    return true;

}

//copy file to rw layers
bool copyFile2RW(const char *abs_path, char *resolved){
    if(!xstat(abs_path)){
        strcpy(resolved,abs_path);
        return false;
    }

    if(!is_file_type(abs_path,TYPE_FILE)){
        strcpy(resolved,abs_path);
        return false;
    }

    INITIAL_SYS(fopen)
        INITIAL_SYS(mkdir)

        char rel_path[MAX_PATH];
    char layer_path[MAX_PATH];
    int ret = get_relative_path_layer(abs_path, rel_path, layer_path);
    const char * container_root = getenv("ContainerRoot");
    if(ret == 0){
        if(strcmp(layer_path, container_root) == 0){
            strcpy(resolved, abs_path);
            return true;
        }else{
            char destpath[MAX_PATH];
            sprintf(destpath,"%s/%s", container_root, rel_path);
            FILE *src, *dest;
            src = real_fopen(abs_path, "r");

            //check if the dest path folder exists
            int mk_ret = recurMkdir(destpath);
            if(mk_ret != 0){
                log_fatal("creating dirs of path: %s encounters failure with error %s", destpath, strerror(errno));
                return false;
            }

            if(xstat(destpath)){
                //truncate and rewrite
                dest = real_fopen(destpath, "w");
            }else{
                dest = real_fopen(destpath, "w+");
            }
            if(src == NULL || dest == NULL){
                log_fatal("open file encounters error, src: %s -> %p, dest: %s -> %p", abs_path,src, destpath,dest);
                return -1;
            }
            char ch = fgetc(src);
            while(ch != EOF){
                fputc(ch,dest);
                ch = fgetc(src);
            }
            fclose(src);
            fclose(dest);
            log_debug("finished copying from %s to %s",abs_path,destpath);
            strcpy(resolved,destpath);
            return true;
        }
    }else{
        strcpy(resolved, abs_path);
        return false;
    }
}

/**
 * get any whiteout parent folders
 * if given path is as /f1/f2/f3
 * it will check if /.wh.f1 /f1/.wh.f2 /f1/f2/.wh.f3 exists
 * return: -1 error 0 not exists 1 exists
 */
int getParentWh(const char *abs_path){
    char rel_path[MAX_PATH];
    char layer_path[MAX_PATH];
    int ret = get_relative_path_layer(abs_path,rel_path, layer_path);
    if(ret == 0){
        while(1){
            char whpath[MAX_PATH];
            char * bname = basename(rel_path);
            char * dname = dirname(rel_path);
            if(strcmp(dname,".") == 0){
                sprintf(whpath, "%s/.wh.%s",layer_path, bname);
            }else{
                sprintf(whpath, "%s/%s/.wh.%s",layer_path, dname, bname);
            }
            if(xstat(whpath)){
                return 1;
            }
            if(strcmp(dname,".") == 0){
                break;
            }
        }
        return 0;
    }else{
        log_fatal("get relative path encounters error, layer path: %s, input path: %s", layer_path, abs_path);
        return -1;
    }
}

bool xstat(const char *abs_path){
    if(abs_path == NULL || *abs_path == '\0'){
        return false;
    }
    INITIAL_SYS(__xstat)
        struct stat st;
    if(real___xstat(1,abs_path,&st) == 0){
        return true;
    }
    return false;
}

bool lxstat(const char *abs_path){
    if(abs_path == NULL || *abs_path == '\0'){
        return false;
    }
    INITIAL_SYS(__lxstat)
    struct stat st;
    if(real___lxstat(1, abs_path, &st) == 0){
        return true;
    }
    return false;
}

int recurMkdir(const char *path){
    if(path == NULL || *path == '\0' || *path != '/'){
        log_fatal("can't make dir as the input parameter is either null, empty or not absolute path, path: %s", path);
        errno = EEXIST;
        return -1;
    }

    if(strcmp(path,"/") == 0){
        return 0;
    }

    char dname[MAX_PATH];
    strcpy(dname, path);
    dirname(dname);
    if(!xstat(dname)){
        recurMkdir(dname);
    }

    if(!xstat(dname)){
        INITIAL_SYS(mkdir)
            log_debug("start creating dir %s", dname);
        real_mkdir(dname, FOLDER_PERM);
    }
    return 0;
}

//path should be the folder name
int recurMkdirMode(const char *path, mode_t mode){
    if(path == NULL || *path == '\0' || *path != '/'){
        log_fatal("can't make dir as the input parameter is either null, empty or not absolute path, path: %s", path);
        errno = EEXIST;
        return -1;
    }

    if(strcmp(path,"/") == 0){
        return 0;
    }

    if(!xstat(path)){
        char dname[MAX_PATH];
        strcpy(dname, path);
        dirname(dname);
        recurMkdirMode(dname, mode);
    }

    if(!xstat(path)){
        INITIAL_SYS(mkdir)
            log_debug("start creating dir %s", path);
        int ret = real_mkdir(path, mode);
        if(ret != 0){
            log_fatal("creating dirs %s encounters failure with error %s", path, strerror(errno));
            return -1;
        }
    }
    return 0;
}

bool pathExcluded(const char *abs_path){
    if(abs_path == NULL || *abs_path == '\0'){
        return false;
    }

    char resolved[MAX_PATH];
    if(*abs_path != '/'){
        char cwd[MAX_PATH];
        getcwd(cwd, MAX_PATH);
        if(cwd){
            sprintf(resolved, "%s/%s", cwd, abs_path);
            dedotdot(resolved);
        }else{
            log_fatal("could not get cwd");
            return false;
        }
    }else{
        strcpy(resolved, abs_path);
    }

    const char* data_sync = getenv("FAKECHROOT_DATA_SYNC");
    if(data_sync){
        char data_sync_dup[MAX_PATH];
        strcpy(data_sync_dup, data_sync);
        char *str_tmp = strtok(data_sync_dup, ":");
        while(str_tmp){
            if(strncmp(str_tmp, resolved, strlen(str_tmp)) == 0){
                return true;
            }
            str_tmp = strtok(NULL,":");
        }
    }

    const char *exclude_path = getenv("FAKECHROOT_EXCLUDE_PATH");
    if(exclude_path){
        char exclude_path_dup[MAX_PATH];
        strcpy(exclude_path_dup, exclude_path);
        char *str_tmp = strtok(exclude_path_dup,":");
        while (str_tmp){
            if(strncmp(str_tmp, resolved,strlen(str_tmp)) == 0){
                return true;
            }
            str_tmp = strtok(NULL,":");
        }
    }
    return false;
}

//a fixed version of resolving symlink if the target is still symlink, we have to interately resolve it until it targets to the real file
bool iterResolveSymlink(const char *link, char *target){
    log_debug("iterately resolve symlink starts: %s", link);
    //copy link firstly
    char resolved[MAX_PATH];
    strcpy(resolved, link);

    //if resolved is still symlink, solve it until success
    INITIAL_SYS(readlink)
    while(lxstat(resolved) && is_file_type(resolved, TYPE_LINK)){
        char rlink[MAX_PATH];
        ssize_t size = real_readlink(resolved, rlink, MAX_PATH-1);
        if(size == -1){
            log_fatal("can't resolve link %s", resolved);
            strcpy(target,resolved);
            return false;
        }else{
            rlink[size] = '\0';
            char rel_path[MAX_PATH];
            char layer_path[MAX_PATH];
            //we first get absolute path(resolved) rel and layer info 
            int ret = get_relative_path_layer(resolved, rel_path, layer_path);
            log_debug("iterately resolve symlink, ret: %d, original path: %s, rel_path: %s, layer_path: %s", ret, resolved, rel_path, layer_path);
            if(ret == 0){
                //original path is inside container
                
                //starts checking resolved symlink
                if(*rlink != '/'){
                    //here there are 4 different formats 
                    // 1: file -> file2
                    // 2: file -> /path1/file2
                    // 3: file -> bin/file2
                    // 4: file -> ../path1/file2
                    //
                    // 2 is not handled here
                    // we only have to handle 1,3,4 here
                   char abs_path[MAX_PATH];
                   if(strstr(rlink,"/") != NULL && strncmp(rlink,"..",2) !=0 ){
                       //handle 3
                       sprintf(abs_path, "/%s", rlink);
                   }else if(strncmp(rlink,"..",2) == 0 && strstr(rlink, "/") != NULL){
                       //handle 4
                       //get parent folder
                       char parent[MAX_PATH];
                       char base[MAX_PATH];
                       bool ret = split_path(rel_path, parent, base);
                       //get original path parent folder and base file
                       log_debug("resolve symlink splits path: %s, parent: %s, base: %s", rel_path, parent, base);
                       if(ret){
                            if(*rel_path == '.'){
                                sprintf(abs_path, "%s", rlink + 2);
                            }else{
                                sprintf(abs_path, "/%s/%s", parent, rlink);
                            }
                       }else{
                            sprintf(abs_path, "%s", rlink + 2);
                       }
                   }else{
                       //handle 1
                       if(*rel_path == '.'){
                           sprintf(abs_path, "/%s", resolved);
                       }else{
                           char parent[MAX_PATH];
                           char base[MAX_PATH];
                           bool ret = split_path(rel_path, parent, base);
                           log_debug("resolve symlink splits path: %s, parent: %s, base: %s", rel_path, parent, base);
                           if(ret){
                               sprintf(abs_path, "/%s/%s", parent, rlink);
                           }else{
                               sprintf(abs_path, "/%s", rlink);
                           }
                       }
                   }
                   //reset and copy to resolved
                   memset(resolved, '\0', MAX_PATH);
                   strcpy(resolved, abs_path);
                   //dedotdot resolved path
                   dedotdot(resolved);
                   // here we convert path to absolute path according to container base path. i.e, /p1/f1
                }else{
                    memset(resolved, '\0', MAX_PATH);
                    strcpy(resolved, rlink);
                    dedotdot(resolved);
                }

                log_debug("resolve symlink step 1: resolved: %s", resolved);
                //then we start finding the correct absolute path based on container layers
                if(pathExcluded(resolved)){
                    strcpy(target,resolved);
                    return true;
                }else{
                    char ** paths;
                    size_t num;
                    paths = getLayerPaths(&num);
                    bool b_resolved = false;
                    if(num > 0){
                        char tmp[MAX_PATH];
                        for(size_t i = 0; i< num; i++){
                            memset(tmp,'\0',MAX_PATH);
                            if(*resolved == '/'){
                                sprintf(tmp, "%s%s", paths[i],resolved);
                            }else{
                                sprintf(tmp, "%s/%s", paths[i],resolved);
                            }
                            if(!lxstat(tmp)){
                                log_debug("symlink failed resolved: %s",tmp);
                                if(getParentWh(tmp)){
                                    break;
                                }
                                continue;
                            }else{
                                log_debug("symlink successfully resolved: %s",tmp);
                                b_resolved = true;

                                //reset and copy
                                memset(resolved,'\0', MAX_PATH);
                                strcpy(resolved,tmp);
                                break;
                            }
                        }
                    }
                    if(paths){
                        for(int i = 0; i < num; i++){
                            debug_free(paths[i]);
                        }
                        debug_free(paths);
                    }
                    if(!b_resolved){
                        const char * container_root = getenv("ContainerRoot");
                        char tmp[MAX_PATH];
                        if(*resolved == '/'){
                            snprintf(tmp, MAX_PATH,"%s%s",container_root,resolved);
                        }else{
                            snprintf(tmp, MAX_PATH,"%s/%s",container_root,resolved);
                        }

                        memset(resolved,'\0',MAX_PATH);
                        strcpy(resolved,tmp);
                    }
                }// find real absolute path

            }else{
                //original path is not inside container, meaning that it is either excluded path or escapted path
                if(!pathExcluded(resolved)){
                    log_fatal("iterately resolve symlink path: %s escaped from container", resolved);
                    strcpy(target, resolved);
                    return false;
                }
            }

        } // size == -1 else ends
    }//while ends

    strcpy(target, resolved);
    return true;
}

//this function is used for resolving symlink on different situations
// 1: file -> file2
// 2: file -> /path1/file2
// 3: file -> bin/file2
// 4: file -> ../path1/file2
bool resolveSymlink(const char *link, char *target){
    log_debug("resolve symlink starts: %s", link);
    if(lxstat(link) && is_file_type(link, TYPE_LINK)){
        INITIAL_SYS(readlink)
        char resolved[MAX_PATH];
        ssize_t size = real_readlink(link, resolved, MAX_PATH-1);
        if(size == -1){
            log_fatal("can't resolve link %s", link);
            strcpy(target,link);
            return false;
        }else{
            resolved[size] = '\0';
            char rel_path[MAX_PATH];
            char layer_path[MAX_PATH];
            int ret = get_relative_path_layer(link, rel_path, layer_path);
            log_debug("resolve symlink, ret: %d, resolved: %s, abs_path: %s, rel_path: %s, layer_path: %s", ret, resolved, link, rel_path, layer_path);
            if(ret == 0){
                if(*resolved != '/'){
                    //here there are 4 different formats 
                    // 1: file -> file2
                    // 2: file -> /path1/file2
                    // 3: file -> bin/file2
                    // 4: file -> ../path1/file2
                    //
                    // 2 is not handled here
                    // we only have to handle 1,3,4 here
                   char abs_path[MAX_PATH];
                   if(strstr(resolved,"/") != NULL && strncmp(resolved,"..",2) !=0 ){
                       //handle 3
                       sprintf(abs_path, "/%s", resolved);
                   }else if(strncmp(resolved,"..",2) == 0 && strstr(resolved, "/") != NULL){
                       //handle 4
                       //get parent folder
                       char parent[MAX_PATH];
                       char base[MAX_PATH];
                       bool ret = split_path(rel_path, parent, base);
                       log_debug("resolve symlink splits path: %s, parent: %s, base: %s", rel_path, parent, base);
                       if(ret){
                            if(*rel_path == '.'){
                                sprintf(abs_path, "/%s", resolved + 2);
                            }else{
                                sprintf(abs_path, "/%s/%s", parent, resolved);
                            }
                       }else{
                            sprintf(abs_path, "/%s", resolved + 2);
                       }
                   }else{
                       //handle 1
                       if(*rel_path == '.'){
                           sprintf(abs_path, "/%s", resolved);
                       }else{
                           char parent[MAX_PATH];
                           char base[MAX_PATH];
                           bool ret = split_path(rel_path, parent, base);
                           log_debug("resolve symlink splits path: %s, parent: %s, base: %s", rel_path, parent, base);
                           if(ret){
                               sprintf(abs_path, "/%s/%s", parent, resolved);
                           }else{
                               sprintf(abs_path, "/%s", resolved);
                           }
                       }
                   }
                   //reset and copy to resolved
                   memset(resolved, '\0', MAX_PATH);
                   strcpy(resolved, abs_path);
                   //dedotdot resolved path
                   dedotdot(resolved);
                } // here we convert path to absolute path according to container base path. i.e, /p1/f1


                //then we start finding the correct absolute path based on container layers
                if(pathExcluded(resolved)){
                    strcpy(target,resolved);
                    return true;
                }else{
                    char ** paths;
                    size_t num;
                    paths = getLayerPaths(&num);
                    bool b_resolved = false;
                    if(num > 0){
                        char tmp[MAX_PATH];
                        for(size_t i = 0; i< num; i++){
                            memset(tmp,'\0',MAX_PATH);
                            if(*resolved == '/'){
                                sprintf(tmp, "%s%s", paths[i],resolved);
                            }else{
                                sprintf(tmp, "%s/%s", paths[i],resolved);
                            }
                            if(!xstat(tmp)){
                                log_debug("symlink failed resolved: %s",tmp);
                                if(getParentWh(tmp)){
                                    break;
                                }
                                continue;
                            }else{
                                log_debug("symlink successfully resolved: %s",tmp);
                                char tmp_solved[MAX_PATH];
                                snprintf(tmp_solved,MAX_PATH,"%s",tmp);
                                b_resolved = true;
                                strcpy(target,tmp_solved);
                                break;
                            }
                        }
                    }
                    if(paths){
                        for(int i = 0; i < num; i++){
                            debug_free(paths[i]);
                        }
                        debug_free(paths);
                    }
                    if(!b_resolved){
                        const char * container_root = getenv("ContainerRoot");
                        char tmp[MAX_PATH];
                        if(*resolved == '/'){
                            snprintf(tmp, MAX_PATH,"%s%s",container_root,resolved);
                        }else{
                            snprintf(tmp, MAX_PATH,"%s/%s",container_root,resolved);
                        }
                        strcpy(target,tmp);
                        return false;
                    }
                    return true;
                }
            }else{
                //not inside container
                if(!pathExcluded(link)){
                    log_fatal("resolve symlink path: %s escaped from container", link);
                    strcpy(target, link);
                    return false;
                }
            }
        }
    }else{
        strcpy(target, link);
        return false;
    }
}


//in the root path of any layers
bool is_container_root(const char *abs_path){
    if(abs_path == NULL || *abs_path == '\0'){
        return false;
    }
    if(*abs_path != '/'){
        log_error("input path should be absolute path rather than relative path");
        return false;
    }
    char rel_path[MAX_PATH];
    char layer_path[MAX_PATH];
    int ret = get_relative_path_layer(abs_path, rel_path, layer_path);
    if(ret == 0 && strcmp(rel_path, ".") == 0){
        return true;
    }
    return false;
}

bool is_inside_container(const char *abs_path){
    if(abs_path == NULL || *abs_path == '\0'){
        return false;
    }
    if(*abs_path != '/'){
        log_error("input path should be absolute path rather than relative path");
        return false;
    }
    char rel_path[MAX_PATH];
    char layer_path[MAX_PATH];
    int ret = get_relative_path_layer(abs_path, rel_path, layer_path);
    if(ret == 0){
        return true;
    }
    return false;
}

bool pathIncluded(const char *abs_path){
    if(abs_path == NULL || *abs_path == '\0'){
        return false;
    }
    if(*abs_path != '/'){
        log_error("input path should be absolute path rather than relative path");
        return false;
    }
    const char *include_path= getenv("FAKECHROOT_INCLUDE_PATH");
    if(include_path){
        char include_path_dup[MAX_PATH];
        strcpy(include_path_dup, include_path);
        char *str_tmp = strtok(include_path_dup,":");
        while (str_tmp){
            if(strncmp(str_tmp, abs_path,strlen(str_tmp)) == 0){
                return true;
            }
            str_tmp = strtok(NULL,":");
        }
    }
    return false;

}

bool split_path(const char *path, char *parent, char *base){
    if(path == NULL || *path == '\0'){
        return false;
    }
    char *ret = strrchr(path, '/');
    if(ret == NULL){
        return false;
    }
    memset(parent, '\0', MAX_PATH);
    memset(base, '\0', MAX_PATH);
    strncpy(parent, path, (strlen(path) - strlen(ret)));
    strcpy(base, ret+1);
    return true;
}

bool str_in_array(const char *str, const char **array, int num){
    if(str == NULL || array == NULL || num <= 0){
        return false;
    }

    for(int i = 0; i< num; i++){
        if(strncmp(str, array[i], strlen(array[i])) == 0){
            return true;
        }
    }
    return false;
}

/**----------------------------------------------------------------------------------**/
int fufs_open_impl(const char* function, ...){
    int dirfd = -1;
    const char *path;
    int oflag;
    mode_t mode;

    va_list args;
    va_start(args,function);
    if(strcmp(function,"openat") == 0 || strcmp(function,"openat64") == 0){
        dirfd = va_arg(args,int);
    }

    path = va_arg(args, const char *);
    oflag = va_arg(args, int);
    mode = va_arg(args, mode_t);
    va_end(args);

    INITIAL_SYS(open)
    INITIAL_SYS(openat)
    INITIAL_SYS(open64)
    INITIAL_SYS(openat64)

    char destpath[MAX_PATH];
    strcpy(destpath, path);
    //not exists or excluded directly calling real open
    if(!lxstat(path) || pathExcluded(path)){
        log_debug("open path: %s could not be found", path);
        if(oflag & O_DIRECTORY){
            goto end_folder;
        }
        goto end_file;
    }else{
        //check if it is symlink
        if(is_file_type(path, TYPE_LINK)){
            char link_resolved[MAX_PATH];
            if(!resolveSymlink(path, link_resolved)){
                goto err;
            }
            memset(destpath, '\0', MAX_PATH);
            strcpy(destpath, link_resolved);
            log_debug("open resolves link: %s, target: %s", path, destpath);
        }

        //if it exists, then copy and write
        char rel_path[MAX_PATH];
        char layer_path[MAX_PATH];
        int ret = get_relative_path_layer(destpath, rel_path, layer_path);
        if(ret == 0){
            const char * container_root = getenv("ContainerRoot");
            if(strcmp(layer_path,container_root) == 0){
                if(oflag & O_DIRECTORY){
                    goto end_folder;
                }
                goto end_file;
            }else{
                //exist other layers
                //
                //if target is folder
                if((oflag & O_DIRECTORY) || (xstat(destpath) && is_file_type(destpath,TYPE_DIR))){
                    goto end_folder;
                }

                //read only
                if(oflag == 0){
                    goto end_file;
                }

                //copy and write
                if(xstat(destpath) && is_file_type(destpath, TYPE_FILE)){
                    char oldpath[MAX_PATH];
                    strcpy(oldpath, destpath);
                    memset(destpath,'\0',MAX_PATH);
                    if(!copyFile2RW(oldpath, destpath)){
                        log_fatal("copy from %s to %s encounters error", oldpath, destpath);
                        return -1;
                    }
                }
                goto end;
            }
        }else{
            log_fatal("%s file doesn't exist in container", destpath);
            return -1;
        }
    }

end_folder:
    if(!xstat(destpath) && (oflag & O_WRONLY || oflag & O_RDWR)){
        INITIAL_SYS(mkdir)
            int ret = recurMkdirMode(destpath,FOLDER_PERM);
        if(ret != 0){
            log_fatal("creating dirs %s encounters failure with error %s", destpath, strerror(errno));
            return -1;
        }
    }
    goto end;


end_file:
    if(!xstat(destpath) && (oflag & O_WRONLY || oflag & O_RDWR)){
        INITIAL_SYS(mkdir)
            char dname[MAX_PATH];
        strcpy(dname,destpath);
        dirname(dname);
        int ret = recurMkdirMode(dname,FOLDER_PERM);
        if(ret != 0){
            log_fatal("creating dirs %s encounters failure with error %s", dname, strerror(errno));
            return -1;
        }
    }
    goto end;

end:
    log_debug("%s %s ends", function, destpath);
    if(strcmp(function,"openat") == 0){
        return RETURN_SYS(openat,(dirfd,destpath,oflag,mode))
    }
    if(strcmp(function,"open") == 0){
        return RETURN_SYS(open,(destpath,oflag,mode))
    }
    if(strcmp(function,"openat64") == 0){
        return RETURN_SYS(openat64,(dirfd,destpath,oflag,mode))
    }
    if(strcmp(function,"open64") == 0){
        return RETURN_SYS(open64,(destpath,oflag,mode))
    }

err:
    errno = EACCES;
    return -1;
}

FILE* fufs_fopen_impl(const char * function, ...){
    const char *path;
    const char *mode;
    FILE *stream;

    va_list args;
    va_start(args,function);
    path = va_arg(args, const char *);
    mode = va_arg(args, const char *);

    if(strcmp(function,"freopen") == 0 || strcmp(function,"freopen64") == 0){
        stream= va_arg(args,FILE *);
    }
    va_end(args);

    INITIAL_SYS(fopen)
    INITIAL_SYS(fopen64)
    INITIAL_SYS(freopen)
    INITIAL_SYS(freopen64)

    char destpath[MAX_PATH];
    strcpy(destpath, path);

    if(!lxstat(path) || pathExcluded(path)){
        goto end;
    }else{
        //check if it is symlink
        if(is_file_type(path, TYPE_LINK)){
            char link_resolved[MAX_PATH];
            if(!resolveSymlink(path, link_resolved)){
                goto err;
            }
            memset(destpath, '\0', MAX_PATH);
            strcpy(destpath, link_resolved);
            log_debug("fopen resolves link: %s, target: %s", path, destpath);
        }

        char rel_path[MAX_PATH];
        char layer_path[MAX_PATH];
        int ret = get_relative_path_layer(destpath, rel_path, layer_path);
        if(ret == 0){
            const char * container_root = getenv("ContainerRoot");
            if(strcmp(layer_path,container_root) == 0){
                goto end;
            }else{
                //copy and write
                if(strncmp(mode,"r",1) == 0){
                    goto end;
                }

                char oldpath[MAX_PATH];
                //backup
                strcpy(oldpath, destpath);
                memset(destpath,'\0',MAX_PATH);
                if(!copyFile2RW(oldpath, destpath)){
                    log_fatal("copy from %s to %s encounters error", oldpath, destpath);
                    return NULL;
                }
                if(strcmp(function,"fopen") == 0){
                    return RETURN_SYS(fopen,(destpath,mode))
                }
                if(strcmp(function,"fopen64") == 0){
                    return RETURN_SYS(fopen64,(destpath,mode))
                }
                if(strcmp(function,"freopen") == 0){
                    return RETURN_SYS(freopen,(destpath,mode,stream))
                }
                if(strcmp(function,"freopen64") == 0){
                    return RETURN_SYS(freopen64,(destpath,mode,stream))
                }
                goto err;
            }
        }else{
            log_fatal("%s file doesn't exist in container", destpath);
            return NULL;
        }

    }

end:
    log_debug("%s ends", function);
    if(!xstat(destpath)){
        INITIAL_SYS(mkdir)
        recurMkdir(destpath);
    }

    if(strcmp(function,"fopen") == 0){
        return RETURN_SYS(fopen,(destpath,mode))
    }
    if(strcmp(function,"fopen64") == 0){
        return RETURN_SYS(fopen64,(destpath,mode))
    }
    if(strcmp(function,"freopen") == 0){
        return RETURN_SYS(freopen,(destpath,mode,stream))
    }
    if(strcmp(function,"freopen64") == 0){
        return RETURN_SYS(freopen64,(destpath,mode,stream))
    }

err:
    errno = EACCES;
    return NULL;
}

int fufs_unlink_impl(const char* function,...){
    va_list args;
    va_start(args,function);
    int dirfd = -1;
    const char *abs_path;
    int oflag;

    if(strcmp(function,"unlinkat") == 0){
        dirfd = va_arg(args,int);
        abs_path = va_arg(args, const char *);
        oflag = va_arg(args, int);
    }else{
        abs_path = va_arg(args, const char *);
    }
    va_end(args);

    INITIAL_SYS(unlink)
        INITIAL_SYS(unlinkat)

        if(!lxstat(abs_path)){
            errno = ENOENT;
            return -1;
        }else if(pathExcluded(abs_path)){
            goto end;
        }else{
            //check if deleting folder with all .wh files inside?
            if(is_file_type(abs_path, TYPE_DIR)){
                char **names;
                size_t num;
                getDirentsOnlyNames(abs_path, &names,&num);
                bool is_all_wh = false;
                for(size_t i = 0;i<num;i++){
                    if(strncmp(names[i],".wh",3) == 0){
                        if(i == 0){
                            is_all_wh = true;
                        }else{
                            is_all_wh = is_all_wh & true;
                        }
                    }else{
                        is_all_wh = false;
                        break;
                    }
                }
                if(is_all_wh){
                    log_debug("all files in folder: %s are whiteout files, will entirely delete everything",abs_path);
                    char tmp[MAX_PATH];
                    for(size_t i = 0; i<num; i++){
                        sprintf(tmp,"%s/%s",abs_path,names[i]);
                        log_debug("all files in folder: %s are whiteout files, delete target item: %s",abs_path, tmp);
                        real_unlink(tmp);
                    }
                }else{
                    char tmp[MAX_PATH];
                    for(size_t i = 0; i<num; i++){
                        sprintf(tmp,"%s/%s",abs_path,names[i]);
                        unlink(tmp);
                    }
                }

                //clean up
                if(names){
                    for(int i = 0; i < num; i++){
                        if(names[i]){
                            debug_free(names[i]);
                        }
                    }
                    debug_free(names);
                }
            }

            char rel_path[MAX_PATH];
            char layer_path[MAX_PATH];
            int ret = get_relative_path_layer(abs_path,rel_path, layer_path);
            if(ret == -1){
                log_fatal("request path is not in container, path: %s", abs_path);
                return -1;
            }
            const char * root_path = getenv("ContainerRoot");

            char * bname = basename(rel_path);
            char dname[MAX_PATH], dname_cp[MAX_PATH];
            strcpy(dname, rel_path);
            strcpy(dname_cp, rel_path);
            dirname(dname);
            //fix bug that could not remove folders inside root folder
            if(strcmp(dname, dname_cp) == 0){
                strcpy(dname,".");
            }

            //if remove .wh file
            if(strncmp(bname,".wh",3) == 0){
                goto end;
            }


            INITIAL_SYS(creat)
                if(strcmp(root_path, layer_path) == 0){
                    //if file does not exist in other layers, then we directly delete them
                    char layers_resolved[MAX_PATH];
                    if(!findFileInLayersSkip(abs_path, layers_resolved, 1)){
                        goto end;
                    }

                    char whpath[MAX_PATH];
                    if(strcmp(dname, ".") == 0){
                        sprintf(whpath,"%s/.wh.%s",root_path,bname);
                    }else{
                        sprintf(whpath,"%s/%s/.wh.%s",root_path,dname,bname);
                    }
                    if(!xstat(whpath)){
                        int fd = real_creat(whpath,FILE_PERM);
                        if(fd < 0){
                            log_fatal("%s can't create file: %s with error: %s", function, whpath, strerror(errno));
                            return -1;
                        }
                        close(fd);
                    }
                    goto end;
                }else{
                    //request path is in other layers rather than rw layer
                    char whpath[MAX_PATH];
                    if(strcmp(dname, ".") == 0){
                        sprintf(whpath,"%s/.wh.%s",root_path,bname);
                    }else{
                        sprintf(whpath,"%s/%s",root_path,dname);
                        if(!xstat(whpath)){
                            recurMkdirMode(whpath,FOLDER_PERM);
                        }
                        sprintf(whpath,"%s/.wh.%s", whpath,bname);
                    }
                    int fd = real_creat(whpath, FILE_PERM);
                    if(fd < 0){
                        log_fatal("%s can't create file: %s", function, whpath);
                        return -1;
                    }
                    close(fd);
                    //do not goto end, it will remove the real one in other layers.
                    return 0;
                }
        }

end:
    log_debug("%s ends on %s", function, abs_path);
    //if dir should use rmdir
    if(is_file_type(abs_path, TYPE_DIR)){
        return rmdir(abs_path);
    }
    if(strcmp(function,"unlinkat") == 0){
        return RETURN_SYS(unlinkat,(dirfd,abs_path,oflag))
    }else{
        return RETURN_SYS(unlink,(abs_path))
    }
}

//this function scans content of given directory, and returns its content without whiteouted files
struct dirent_obj* listDir(const char *path, int *num){
    size_t layer_num;
    char ** layers = getLayerPaths(&layer_num);
    if(layer_num < 1){
        log_fatal("can't find layer info");
        return NULL;
    }

    struct dirent_obj *head, *tail;
    head = tail = NULL;

    //map
    hmap_t* dirent_map = create_hmap(MAX_ITEMS);
    hmap_t* wh_map = create_hmap(MAX_ITEMS);

    char rel_path[MAX_PATH];
    char layer_path[MAX_PATH];
    int ret = get_relative_path_layer(path, rel_path, layer_path);
    if (ret == -1) {
        log_fatal("%s is not inside the container, abs path: %s", rel_path, path);
        return NULL;
    }

    //used for garbage collected dirent_obj
    struct dirent_obj* gar_head, *gar_tail;
    gar_head = gar_tail = NULL;

    for (int i = 0; i < layer_num; i++) {
        char each_layer_path[MAX_PATH];
        //search in each layer
        sprintf(each_layer_path, "%s/%s", layers[i], rel_path);
        log_debug("preparing for accessing target layer: %s", each_layer_path);

        if(xstat(each_layer_path)){
            struct dirent_obj* items, *wh_items;
            items = wh_items = NULL;
            size_t num, wh_num;
            getDirentsWhNoRet(each_layer_path, &items, &num, &wh_items, &wh_num);
            if(items || wh_items){
                //initialization for the first rw layer
                //add all items to it, tail->next is always NULL. tail is the last item
                if (head == NULL && tail == NULL && is_empty_hmap(wh_map) && is_empty_hmap(dirent_map)){
                    head = tail = items;
                    if(head){
                        while (tail->next != NULL) {
                            log_debug("item added to dirent_map %s", tail->d_name);
                            add_item_hmap(dirent_map, tail->d_name, NULL);
                            tail = tail->next;
                        }
                        log_debug("item added to dirent_map %s", tail->d_name);
                        add_item_hmap(dirent_map, tail->d_name, NULL);
                    }

                    //add wh_items to hashmap and then free them
                    if(wh_items){
                        gar_head = wh_items;
                    }
                    while(wh_items){
                        log_debug("item added to wh_map %s", wh_items->d_name);
                        add_item_hmap(wh_map, wh_items->d_name, NULL);
                        wh_items = wh_items->next;
                        if(wh_items && wh_items->next == NULL){
                            gar_tail = wh_items;
                        }
                    }
                    //otherwise merge content
                } else{
                    //save prew
                    if(tail){
                        //tail is not NULL, meaning that head is not NULL, head should not be reset
                        //here we try to connect previous chain with new item
                        while(items){
                            if(!contain_item_hmap(dirent_map, items->d_name) && !contain_item_hmap(wh_map, items->d_name)){
                                log_debug("item added to dirent_map %s", items->d_name);
                                add_item_hmap(dirent_map, items->d_name, NULL);
                                tail->next = items; //link the new item
                                tail = tail->next; //tail becomes the linked item
                                items = items->next;  //items points the latest candidate
                            }else{
                                //free it
                                struct dirent_obj* p = items;
                                items = items->next;
                                debug_free(p->dp);
                                debug_free(p->dp64);
                                debug_free(p);
                            }
                        }
                    }else{
                        //tail is NULL, meaning that head must be ULL as well
                        while(items){
                            if(!contain_item_hmap(dirent_map, items->d_name) && !contain_item_hmap(wh_map, items->d_name)){
                                log_debug("item added to dirent_map %s", items->d_name);
                                add_item_hmap(dirent_map, items->d_name, NULL);
                                if(!head){
                                    //no previous item, set new item as head and tail
                                    head = tail = items;
                                }else{
                                    tail->next = items;
                                    tail = tail->next;
                                }
                                items = items->next;
                            }else{
                                //free it
                                struct dirent_obj* p = items;
                                items = items->next;
                                debug_free(p->dp);
                                debug_free(p->dp64);
                                debug_free(p);
                            }
                        }
                    }
                    if(tail){
                        tail->next = NULL;
                    }

                    //here we check wh_items
                    if(wh_items){
                        if(gar_head == NULL && gar_tail == NULL){
                            gar_head = wh_items;
                        }else{
                            gar_tail->next = wh_items;
                        }
                    }
                    while(wh_items){
                        log_debug("item added to wh_map %s", wh_items->d_name);
                        add_item_hmap(wh_map, wh_items->d_name, NULL);
                        wh_items = wh_items->next;
                        if(wh_items && wh_items->next == NULL){
                            gar_tail = wh_items;
                        }
                    }
                } //else merge content ends

            } //layer has content?
        } // layer exists?

        //if nay whiteout file for parant folder exists
        if(getParentWh(each_layer_path) == 1){
            break;
        }
    }// loop layers

    //free layers and hashmap
    while(gar_head){
        struct dirent_obj* p = gar_head;
        gar_head = gar_head->next;
        debug_free(p->dp);
        debug_free(p->dp64);
        debug_free(p);
    }

    for(size_t i = 0; i< layer_num; i++){
        debug_free(layers[i]);
    }
    debug_free(layers);

    if(dirent_map){
        destroy_hmap(dirent_map);
    }

    if(wh_map){
        destroy_hmap(wh_map);
    }

    //count
    *num = 0;
    struct dirent_obj* p = head;
    while(p){
        (*num)++;
        p = p->next;
    }

    log_debug("listDir ends");
    return head;
}


struct dirent_obj* fufs_opendir_impl(const char* function,...){
    //container layer from top to lower
    va_list args;
    va_start(args,function);
    const char * abs_path = va_arg(args,const char *);
    va_end(args);

    int num;
    struct dirent_obj* ret = listDir(abs_path, &num);
    return ret;
}

int fufs_mkdir_impl(const char* function,...){
    va_list args;
    va_start(args,function);
    int dirfd = -1;
    const char *abs_path;
    mode_t mode;

    if(strcmp(function,"mkdirat") == 0){
        dirfd = va_arg(args,int);
        abs_path = va_arg(args, const char *);
        mode= va_arg(args, mode_t);
    }else{
        abs_path = va_arg(args, const char *);
        mode = va_arg(args, mode_t);
    }
    va_end(args);

    if(pathExcluded(abs_path)){
        log_debug("mkdir %s ends", abs_path);
        return recurMkdirMode(abs_path,mode);
    }
    char rel_path[MAX_PATH];
    char layer_path[MAX_PATH];
    int ret = get_relative_path_layer(abs_path,rel_path,layer_path);
    if (ret == -1){
        log_fatal("%s is not inside the container", rel_path);
        return -1;
    }

    const char * container_root = getenv("ContainerRoot");
    char resolved[MAX_PATH];
    if(strcmp(layer_path,container_root) != 0){
        sprintf(resolved,"%s/%s",container_root,rel_path);
    }else{
        sprintf(resolved,"%s",abs_path);
    }

    dedotdot(resolved);

    log_debug("mkdir %s ends", resolved);
    return recurMkdirMode(resolved, mode);

    /**
      INITIAL_SYS(mkdir)
      INITIAL_SYS(mkdirat)

      if(strcmp(function,"mkdirat") == 0){
      return RETURN_SYS(mkdirat,(dirfd,resolved,mode))
      }else{
      return RETURN_SYS(mkdir,(resolved,mode))
      }
     **/
}

int fufs_link_impl(const char * function, ...){
    va_list args;
    va_start(args,function);
    int olddirfd, newdirfd,flags;
    const char *oldpath,*newpath;
    if(strcmp(function,"linkat") == 0){
        olddirfd = va_arg(args, int);
        oldpath = va_arg(args,const char *);
        newdirfd = va_arg(args, int);
        newpath = va_arg(args, const char *);
        flags = va_arg(args, int);
    }else{
        oldpath = va_arg(args, const char *);
        newpath = va_arg(args, const char *);
    }
    va_end(args);

    //newpath should be changed to rw folder
    char rel_path[MAX_PATH];
    char layer_path[MAX_PATH];
    int ret = get_relative_path_layer(newpath,rel_path,layer_path);
    if (ret == -1){
        log_fatal("%s is not inside the container", rel_path);
        return -1;
    }
    const char * container_root = getenv("ContainerRoot");
    char resolved[MAX_PATH];
    if(strcmp(layer_path,container_root) != 0){
        sprintf(resolved,"%s/%s",container_root,rel_path);
    }else{
        sprintf(resolved,"%s",newpath);
    }

    if(lxstat(resolved)){
        INITIAL_SYS(unlink)
            real_unlink(resolved);
    }

    INITIAL_SYS(linkat)
        INITIAL_SYS(link)

        log_debug("%s ends", function);
    if(strcmp(function,"linkat") == 0){
        return RETURN_SYS(linkat,(olddirfd,oldpath,newdirfd,resolved,flags))
    }else{
        return RETURN_SYS(link,(oldpath,resolved))
    }
}

int fufs_symlink_impl(const char *function, ...){
    va_list args;
    va_start(args,function);
    const char *target, *linkpath;
    int newdirfd;
    if(strcmp(function,"symlinkat") == 0){
        target = va_arg(args, const char *);
        newdirfd = va_arg(args, int);
        linkpath = va_arg(args, const char *);
    }else{
        target = va_arg(args, const char *);
        linkpath = va_arg(args, const char *);
    }
    va_end(args);

    //check the linkpath whether locating inside rw folder
    char resolved[MAX_PATH];
    char rel_path[MAX_PATH];
    char layer_path[MAX_PATH];
    int ret = get_relative_path_layer(linkpath,rel_path,layer_path);
    if (ret == -1){
        log_fatal("%s is not inside the container", rel_path);
        return -1;
    }
    const char * container_root = getenv("ContainerRoot");
    if(strcmp(layer_path,container_root) != 0){
        sprintf(resolved,"%s/%s",container_root,rel_path);
    }else{
        sprintf(resolved,"%s",linkpath);
    }

    INITIAL_SYS(symlinkat)
    INITIAL_SYS(symlink)

    char dir[MAX_PATH];
    strcpy(dir, resolved);
    dirname(dir);
    //parent folder does not exist
    if(!xstat(dir)){
        recurMkdirMode(dir, FOLDER_PERM);
    }

    log_debug("%s ends on %s => %s", function, target, resolved);
    if(strcmp(function,"symlinkat") == 0){
        return RETURN_SYS(symlinkat,(target,newdirfd,resolved))
    }else{
        return RETURN_SYS(symlink,(target,resolved))
    }
}

int fufs_creat_impl(const char *function,...){
    va_list args;
    va_start(args,function);
    const char *path = va_arg(args, const char *);
    mode_t mode = va_arg(args, mode_t);
    va_end(args);

    char rel_path[MAX_PATH];
    char layer_path[MAX_PATH];
    int ret = get_relative_path_layer(path, rel_path, layer_path);
    if (ret == -1){
        log_fatal("%s is not inside the container", rel_path);
        return -1;
    }
    const char * container_root = getenv("ContainerRoot");
    char resolved[MAX_PATH];
    if(strcmp(layer_path,container_root) != 0){
        sprintf(resolved,"%s/%s",container_root,rel_path);
    }else{
        sprintf(resolved,"%s",path);
    }

    INITIAL_SYS(creat64)
    INITIAL_SYS(creat)

    //create parent folder
    char dir[MAX_PATH];
    strcpy(dir, resolved);
    dirname(dir);
    if(!xstat(dir)){
        recurMkdirMode(dir,FOLDER_PERM);
    }

    log_debug("%s ends", function);
    if(strcmp(function,"creat64") == 0){
        return RETURN_SYS(creat64,(resolved,mode))
    }else{
        return RETURN_SYS(creat,(resolved,mode))
    }
}

int fufs_chmod_impl(const char* function, ...){
    va_list args;
    int fd;
    int flag;
    mode_t mode;
    const char *path;
    va_start(args, function);
    if(strcmp(function,"fchmodat") == 0){
        fd = va_arg(args, int);
        path = va_arg(args, const char *);
        mode = va_arg(args, mode_t);
        flag = va_arg(args, int);
    }else{
        path = va_arg(args,const char *);
        mode = va_arg(args, mode_t);
    }
    va_end(args);

    char rel_path[MAX_PATH];
    char layer_path[MAX_PATH];
    int ret = get_relative_path_layer(path, rel_path, layer_path);
    if(ret == -1){
        errno = EACCES;
        return -1;
    }

    INITIAL_SYS(chmod)
        INITIAL_SYS(lchmod)
        INITIAL_SYS(fchmodat)

        char resolved[MAX_PATH];
    const char * container_root = getenv("ContainerRoot");
    if(strcmp(layer_path,container_root) == 0){
        strcpy(resolved, path);
    }else{
        if(is_file_type(path, TYPE_DIR)){
            const char * container_root = getenv("ContainerRoot");
            char newpath[MAX_PATH];
            sprintf(newpath,"%s/%s", container_root, rel_path);
            if(!xstat(newpath)){
                recurMkdirMode(newpath, FOLDER_PERM);
            }
            strcpy(resolved, newpath);
            goto end;
        }

        if(is_file_type(path, TYPE_FILE) || is_file_type(path, TYPE_LINK)){
            if(!copyFile2RW(path, resolved)){
                log_fatal("copy from %s to %s encounters error", path, resolved);
                return -1;
            }
            goto end;
        }
    }

end:
    log_debug("%s ends", function);
    if(strcmp(function, "chmod") == 0){
        return RETURN_SYS(chmod,(resolved, mode))
    }
    if(strcmp(function, "lchmod") == 0){
        return RETURN_SYS(lchmod,(resolved, mode))
    }
    if(strcmp(function, "fchmodat") == 0){
        return RETURN_SYS(fchmodat,(fd, resolved, mode, flag))
    }
}

int fufs_rmdir_impl(const char* function, ...){
    va_list args;
    va_start(args, function);
    const char * path = va_arg(args, const char *);
    va_end(args);

    if(pathExcluded(path)){
        INITIAL_SYS(rmdir)
        return real_rmdir(path);
    }
    char rel_path[MAX_PATH];
    char layer_path[MAX_PATH];
    int ret = get_relative_path_layer(path, rel_path, layer_path);
    if(ret == -1){
        errno = EACCES;
        return -1;
    }

    INITIAL_SYS(mkdir)
    INITIAL_SYS(creat)

    const char * container_root = getenv("ContainerRoot");

    char * bname = basename(rel_path);
    char dname[MAX_PATH];
    strcpy(dname, rel_path);
    dirname(dname);
    if(strcmp(layer_path,container_root) == 0){
        //two cases
        //1. folder does not exist in another other layers, then delete it directly
        //2. folder exists in other layers, create wh and check all content inside is wh files, clear them
        INITIAL_SYS(rmdir)
        char layers_resolved[MAX_PATH];
        if(!findFileInLayersSkip(path, layers_resolved, 1)){
            goto end;
        }

        char wh[MAX_PATH];
        sprintf(wh,"%s/%s/.wh.%s",container_root,dname,bname);

        char wh_dname[MAX_PATH];
        strcpy(wh_dname, wh);
        dirname(wh_dname);
        if(!xstat(wh_dname)){
            recurMkdirMode(wh_dname,FOLDER_PERM);
        }

        int fd = real_creat(wh,FILE_PERM);
        if(fd < 0){
            log_fatal("%s can't create file: %s with error: %s", function, wh, strerror(errno));
            return -1;
        }
        close(fd);
        if(xstat(path) && is_file_type(path, TYPE_DIR)){
            INITIAL_SYS(unlink)
            char **names;
            size_t num;
            getDirentsOnlyNames(path, &names,&num);
            bool is_all_wh = false;
            for(size_t i = 0;i<num;i++){
                if(strncmp(names[i],".wh",3) == 0){
                    if(i == 0){
                        is_all_wh = true;
                    }else{
                        is_all_wh = is_all_wh & true;
                    }
                }else{
                    is_all_wh = false;
                    break;
                }
            }
            if(is_all_wh){
                log_debug("all files in folder: %s are whiteout files, will entirely delete everything",path);
                char tmp[MAX_PATH];
                for(size_t i = 0; i<num; i++){
                    sprintf(tmp,"%s/%s",path,names[i]);
                    log_debug("all files in folder: %s are whiteout files, delete target item: %s",path, tmp);
                    real_unlink(tmp);
                }
            }

            //clean up
            if(names){
                for(int i = 0; i < num; i++){
                    debug_free(names[i]);
                }
                debug_free(names);
            }
        }

end:
        log_debug("rmdir ends on %s", path);
        return RETURN_SYS(rmdir,(path))
    }else{
        //in other layers
        char new_path[MAX_PATH];
        sprintf(new_path,"%s/%s", container_root,rel_path);
        int ret = recurMkdirMode(new_path,FOLDER_PERM);
        if(ret == 0){
            char wh[MAX_PATH];
            char n_dname[MAX_PATH];
            strcpy(n_dname, new_path);
            dirname(n_dname);
            sprintf(wh,"%s/.wh.%s",n_dname,bname);
            int fd = real_creat(wh, FILE_PERM);
            if(fd < 0){
                log_fatal("%s can't create file: %s with error: %s", function, wh, strerror(errno));
                return -1;
            }
            close(fd);
            log_debug("rmdir ends on %s", new_path);
            return 0;
        }
    }
    errno = EACCES;
    return -1;
}

//rename is used for renaming files
int fufs_rename_impl(const char* function, ...){
    va_list args;
    va_start(args, function);
    int olddirfd, newdirfd;
    const char *oldpath, *newpath;
    if(strcmp(function,"renameat") == 0){
        olddirfd = va_arg(args, int);
        oldpath = va_arg(args, const char *);
        newdirfd = va_arg(args, int);
        newpath = va_arg(args, const char *);
    }else{
        oldpath = va_arg(args, const char *);
        newpath = va_arg(args, const char *);
    }
    va_end(args);
    INITIAL_SYS(creat)

        const char * container_root = getenv("ContainerRoot");
    char old_rel_path[MAX_PATH];
    char old_layer_path[MAX_PATH];
    int old_ret = get_relative_path_layer(oldpath, old_rel_path, old_layer_path);
    if(old_ret != 0 && strncmp(oldpath,"/tmp",strlen("/tmp")) != 0){
        log_fatal("request path is not in container, path: %s", oldpath);
        return -1;
    }
    char old_resolved[MAX_PATH];
    strcpy(old_resolved, oldpath);
    if(old_ret == 0 && strcmp(old_layer_path,container_root) != 0){
        memset(old_resolved, 0, MAX_PATH);
        copyFile2RW(oldpath, old_resolved);
        //fake deleting oldpath
        char * bname = basename(old_resolved);
        char old_resolved_dup[MAX_PATH];
        strcpy(old_resolved_dup, old_resolved);
        dirname(old_resolved_dup);
        char wh_new_rel_path[MAX_PATH];
        char wh_new_layer_path[MAX_PATH];
        get_relative_path_layer(old_resolved_dup, wh_new_rel_path, wh_new_layer_path);

        //get corrected whpath
        char whpath[MAX_PATH];
        sprintf(whpath, "%s/%s/.wh.%s", container_root, wh_new_rel_path, bname);
        dedotdot(whpath);

        if(!xstat(whpath)){
            //create parant folder if needed
            recurMkdir(whpath);
            int fd = real_creat(whpath,FILE_PERM);
            if(fd < 0){
                log_fatal("%s can't create file: %s with error: %s, oldpath: %s, old_rel_path: %s, old_layer_path: %s, ContainerRoot: %s", function, whpath, strerror(errno), oldpath, old_rel_path, old_layer_path, container_root);
                return -1;
            }
            close(fd);
        }
    }

    char new_rel_path[MAX_PATH];
    char new_layer_path[MAX_PATH];
    int new_ret = get_relative_path_layer(newpath, new_rel_path, new_layer_path);
    if(new_ret != 0 && strncmp(newpath, "/tmp", strlen("/tmp")) != 0){
        log_fatal("request path is not in container, path: %s", newpath);
        return -1;
    }

    INITIAL_SYS(rename)
        INITIAL_SYS(renameat)
        log_debug("%s ends", function);
    if(strcmp(new_layer_path, container_root) == 0){
        if(strcmp(function,"renameat") == 0){
            return RETURN_SYS(renameat,(olddirfd,old_resolved,newdirfd,newpath))
        }else{
            return RETURN_SYS(rename,(old_resolved,newpath))
        }
    }else{
        //newpath is not in rw folder, replacing it by force and delete original one
        char new_resolved[MAX_PATH];
        sprintf(new_resolved,"%s/%s",container_root,new_rel_path);
        unlink(newpath);
        if(strcmp(function,"renameat") == 0){
            return RETURN_SYS(renameat,(olddirfd,old_resolved,newdirfd,new_resolved))
        }else{
            return RETURN_SYS(rename,(old_resolved,new_resolved))
        }
    }
    errno = EACCES;
    return -1;
}
