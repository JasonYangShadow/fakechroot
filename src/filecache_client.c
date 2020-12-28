#include "filecache_client.h"
#include "stdlib.h"
#include "libfakechroot.h"
#include "unionfs.h"
#include "log.h"
#include <string.h>

bool readExecMap(char *content){
    INITIAL_SYS(fopen)
    const char *configpath= getenv("ContainerConfigPath");
    FILE *fptr;
    if(configpath && *configpath){
        char path[FAKECHROOT_PATH_MAX];
        sprintf(path, "%s/.execmap", configpath);
        if(xstat(path)){
            if ((fptr = real_fopen(path, "r")) == NULL)
            {
                log_fatal("could not read the file: %s", path);
                goto err;
            }
            fscanf(fptr,"%[^\n]", content);
        }
    }

end:
    if(fptr){
        fclose(fptr);
    }
    return true;

err:
    if(fptr){
        fclose(fptr);
    }
    return false;
}

bool getFCValue(const char* key, char *value){
    char content[FAKECHROOT_PATH_MAX];
    if(readExecMap(content)){
        char* rest = content;
        char* token = NULL;
        while((token = strtok_r(rest, ":", &rest))){
            if(strncmp(token, key, strlen(key)) == 0){
                strcpy(value, token + strlen(key) + 1);
                return true;
            }
        }
    }
    return false;
}