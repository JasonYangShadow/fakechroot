#define _GNU_SOURCE
#ifndef __UNIONFS_H
#define __UNIONFS_H
#include <dirent.h>
#include <stddef.h>
#include <stdbool.h>
#include "hashmap.h"
#include <sys/stat.h>
#include <dlfcn.h>
#include "util.h"
#include "queue.h"
#include "stack.h"

#define MAX_PATH PATH_MAX
#define MAX_ITEMS 1024*2
#define DEFAULT_ITEMS 64
#define MAX_FILENAME 256
#define MAX_VALUE_SIZE 1*1024*1024
#define PREFIX_WH ".wh"
#define MAX_LAYERS 128
#define FOLDER_PERM 0775
#define FILE_PERM 0644
#define LD_MAX_SIZE 8*1024

//macros for system calls
#define DECLARE_SYS(FUNCTION,RETURN_TYPE,ARGS) \
    typedef RETURN_TYPE (*fufs_##FUNCTION) ARGS; \
    static fufs_##FUNCTION real_##FUNCTION = NULL;

#define INITIAL_SYS(FUNCTION) \
    if(!real_##FUNCTION){ \
        real_##FUNCTION = (fufs_##FUNCTION)dlsym(RTLD_NEXT, #FUNCTION); \
    }

#define RETURN_SYS(FUNCTION,ARGS) \
    real_##FUNCTION ARGS;

#define WRAPPER_FUFS(NAME,FUNCTION,...) \
    fufs_##NAME##_impl(#FUNCTION,__VA_ARGS__);

//cross file vars declaration
struct dirent_obj* darr_list[MAX_ITEMS];
enum filetype{TYPE_FILE,TYPE_DIR,TYPE_LINK,TYPE_SOCK};

// declaration ends
    DECLARE_SYS(opendir,DIR*,(const char* name))
    DECLARE_SYS(readdir,struct dirent*,(DIR* dirp))
    DECLARE_SYS(readdir64,struct dirent64*,(DIR* dirp))
    DECLARE_SYS(__xstat,int,(int ver, const char *path, struct stat *buf))
    DECLARE_SYS(__lxstat,int,(int ver, const char *path, struct stat *buf))
    DECLARE_SYS(open,int,(const char *path, int oflag, mode_t mode))
    DECLARE_SYS(openat,int,(int dirfd, const char *path, int oflag, mode_t mode))
    DECLARE_SYS(open64,int,(const char *path, int oflag, mode_t mode))
    DECLARE_SYS(openat64,int,(int dirfd, const char *path, int oflag, mode_t mode))
    DECLARE_SYS(unlink,int,(const char *path))
    DECLARE_SYS(unlinkat,int,(int dirfd, const char *path, int oflag))
    DECLARE_SYS(mkdir,int,(const char *path, mode_t mode))
    DECLARE_SYS(mkdirat,int,(int dirfd, const char *path, mode_t mode))
    DECLARE_SYS(chdir,int,(const char *path))
    DECLARE_SYS(linkat,int,(int olddirfd, const char *oldpath, int newdirfd, const char *newpath, int flags))
    DECLARE_SYS(link,int,(const char *oldpath, const char *newpath))
    DECLARE_SYS(symlinkat,int,(const char *target, int newdirfd, const char *linkpath))
    DECLARE_SYS(symlink,int,(const char *target, const char *linkpath))
    DECLARE_SYS(creat64,int,(const char *path, mode_t mode))
    DECLARE_SYS(creat,int,(const char *path, mode_t mode))
    DECLARE_SYS(chmod,int,(const char *path, mode_t mode))
    DECLARE_SYS(lchmod,int,(const char *path, mode_t mode))
    DECLARE_SYS(fchmodat,int,(int fd, const char *path, mode_t mode, int flag))
    DECLARE_SYS(rmdir,int,(const char *path))
    DECLARE_SYS(rename,int,(const char * oldpath, const char * newpath))
    DECLARE_SYS(renameat,int,(int olddirfd, const char * oldpath, int newdirfd,const char * newpath))
    DECLARE_SYS(renameat2,int,(int olddirfd, const char * oldpath, int newdirfd, const char * newpath, unsigned int flags))
    DECLARE_SYS(fopen,FILE*,(const char * pathname, const char * mode))
    DECLARE_SYS(fopen64,FILE*,(const char * pathname, const char * mode))
    DECLARE_SYS(freopen,FILE*,(const char * pathname, const char * mode, FILE *stream))
    DECLARE_SYS(freopen64,FILE*,(const char * pathname, const char * mode, FILE *stream))
    DECLARE_SYS(readlink,ssize_t,(const char *path, char *buf, size_t bufsiz))
    DECLARE_SYS(execve,int,(const char *filename, char *const argv[], char *const envp[]))
    DECLARE_SYS(getcwd,char*,(char* buf, size_t size))
    DECLARE_SYS(getwd,char*,(char* buf))
    DECLARE_SYS(closedir, int, (DIR* dir))
    DECLARE_SYS(dlopen, void*, (const char *filename, int flag))
    DECLARE_SYS(dlmopen, void*, (Lmid_t lmid, const char *filename, int flags))

    struct dirent_obj {
        struct dirent* dp;
        struct dirent64* dp64;
        char d_name[MAX_FILENAME];
        char abs_path[MAX_PATH];
        struct dirent_obj* next;
    };

enum hash_type{md5,sha256};
DIR * getDirents(const char* name, struct dirent_obj** darr, size_t *num);
void getDirentsNoRet(const char* name, struct dirent_obj** darr, size_t *num);
DIR * getDirentsWh(const char* name, struct dirent_obj** darr, size_t *num, struct dirent_obj** wh_darr, size_t *wh_num);
void getDirentsWhNoRet(const char* name, struct dirent_obj** darr, size_t *num, struct dirent_obj** wh_darr, size_t *wh_num);
void getDirentsOnlyNames(const char* name, char ***names,size_t *num);
char ** getLayerPaths(size_t *num);
char ** getRealLayerPaths(size_t *num);
void filterMemDirents(const char* name, struct dirent_obj* darr, size_t num);
void deleteItemInChain(struct dirent_obj** darr, size_t num);
void deleteItemInChainByPointer(struct dirent_obj** darr, struct dirent_obj** curr);
void addItemToHead(struct dirent_obj** darr, struct dirent* item);
void addItemToHeadV64(struct dirent_obj** darr, struct dirent64* item);
void insertItemToHead(struct dirent_obj**, struct dirent_obj*);
struct dirent * popItemFromHead(struct dirent_obj ** darr);
struct dirent64 * popItemFromHeadV64(struct dirent_obj ** darr);
void clearItems(struct dirent_obj** darr);
//char *struct2hash(void* pointer,enum hash_type type);
int get_abs_path(const char * path, char * abs_path, bool force);
int get_relative_path(const char * path, char * rel_path);
int get_abs_path_base(const char *base, const char *path, char * abs_path, bool force);
int get_relative_path_base(const char *base, const char *path, char * rel_path);
int get_relative_path_layer(const char *path, char * rel_path, char * layer_path);
int narrow_path(char *path);
int append_to_diff(const char* content);
bool is_file_type(const char *path,enum filetype t);
bool transWh2path(const char *name, const char *pre, char *tname);
int getParentWh(const char *abs_path);
bool xstat(const char *abs_path);
bool lxstat(const char *abs_path);
bool pathExcluded(const char *abs_path);
bool pathIncluded(const char *abs_path);
bool findFileInLayers(const char *file,char *resolved);
bool findFileInLayersSkip(const char *file, char *resolved, size_t skip);
bool copyFile2RW(const char *abs_path, char *resolved);
bool resolveSymlink(const char *link, char *target);
bool iterResolveSymlink(const char *link, char *target);
int recurMkdir(const char *path);
int recurMkdirMode(const char *path, mode_t mode);
struct dirent_obj* listDir(const char *path, int *num);
bool is_container_root(const char *abs_path);
bool is_inside_container(const char *abs_path);
bool parse_cmd_line(const char *cmdline, char *execute);
bool split_path(const char *path, char *parent, char *base);
bool str_in_array(const char *str, const char **array, int num);
bool createParentFolder(const char *path);
char** splitStrs(const char*, size_t*, const char*);
int gen_rpath_from_file(const char *src, char *ret_rpath);
int gen_needed_libs_from_file(const char *src, char *libs);
int gen_tree_dependency(Queue *q, Stack *st);
int find_library(char *lib_name, char *rpath);
bool get_path_from_fd(int fd, char *path);

//fake union fs functions
int fufs_chdir_impl(const char * function, ...);
int fufs_creat_impl(const char *function,...);
int fufs_link_impl(const char * function, ...);
int fufs_mkdir_impl(const char* function,...);
int fufs_open_impl(const char* function, ...);
struct dirent_obj* fufs_opendir_impl(const char* function,...);
int fufs_symlink_impl(const char *function, ...);
int fufs_unlink_impl(const char* function,...);
int fufs_chmod_impl(const char* function, ...);
int fufs_rmdir_impl(const char* function, ...);
int fufs_rename_impl(const char* function, ...);
FILE* fufs_fopen_impl(const char * function,...);
#endif
