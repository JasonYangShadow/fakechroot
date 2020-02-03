#include <config.h>
#define _GNU_SOURCE
#include <sys/mman.h>
#include <dlfcn.h>
#include "unionfs.h"
#include "setenv.h"
#include "log.h"

int __libc_start_main(int (*main) (int,char **,char **),int argc,char **ubp_av,void (*init) (void),void (*fini)(void),void (*rtld_fini)(void),void (*stack_end)) {
    int (*original__libc_start_main)(int (*main) (int,char **,char **),int argc,char **ubp_av,void (*init) (void),void (*fini)(void),void (*rtld_fini)(void),void (*stack_end));

    log_debug("--libc_start_main starts, ubp_av: %p", ubp_av);
    if(ubp_av[0]){
        log_debug("__libc_start_main init, argv[0] = %s", ubp_av[0]); 
        char tmp[PATH_MAX];
        strcpy(tmp, ubp_av[0]);
        int ret = narrow_path(tmp);
        if(ret == 0){
            log_debug("__libc_start_main narrows down argv0 path in main from: %s -> %s", ubp_av[0], tmp);
            strcpy(ubp_av[0], tmp);
        }
    }

    original__libc_start_main = dlsym(RTLD_NEXT,"__libc_start_main");
    return original__libc_start_main(main,argc,ubp_av,init,fini,rtld_fini,stack_end);
}
