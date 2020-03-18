#include "elf_reader.h"

//******************32 bit head *****************************
bool validELF(Elf32_Ehdr *hd){
    if(hd){
        if(!strncmp((char *)(hd->e_ident),"\177ELF",4)){
            return true;
        }
    }
    return false;
}

//test if ELF64 and shared type
bool validType(Elf32_Ehdr *hd){
    if(hd){
        if(hd->e_ident[EI_CLASS] == ELFCLASS64 && hd->e_type == ET_DYN){
            return true;
        }
    }
    return false;
}

int read_elf_header32(int fd, Elf32_Ehdr *hd){
    if(lseek(fd, (off_t)0, SEEK_SET) == (off_t)0){
        if(read(fd,(void *)hd, sizeof(Elf32_Ehdr)) == sizeof(Elf32_Ehdr)){
            return 0;
        }
    }
    return -1;
}
//******************32 bit head *****************************

int read_elf_header64(int fd, Elf64_Ehdr *hd){
    if(lseek(fd, (off_t)0, SEEK_SET) == (off_t)0){
        if(read(fd,(void *)hd, sizeof(Elf64_Ehdr)) == sizeof(Elf64_Ehdr)){
            return 0;
        }
    }
    return -1;
}

int getAllSectionHeaders(int fd, Elf64_Ehdr *head, Elf64_Shdr ***shead){
    if(!head){
        *shead = NULL;
        return -1;
    }

    //lseek first
    if(lseek(fd, (off_t)head->e_shoff, SEEK_SET) == (off_t)head->e_shoff){
        *shead = (Elf64_Shdr **)malloc(head->e_shnum * sizeof(Elf64_Shdr *));
        for(int i = 0;i <head->e_shnum; i++){
            (*shead)[i] = (Elf64_Shdr *)malloc(head->e_shentsize);
            if(read(fd,(*shead)[i], head->e_shentsize) != head->e_shentsize){
                goto cleanup;
            }
        }

        if(*shead){
            return head->e_shnum;
        }
    }

cleanup:
    if(*shead){
        for(int i = 0; i<head->e_shnum; i++){
            if((*shead)[i]){
                free((*shead)[i]);
            }
        }
        free(*shead);
    }

    *shead = NULL;
    return -1;
}

Elf64_Shdr* findSectionHead(int fd, Elf64_Ehdr *head, Elf64_Shdr** stbl, int sh_num, char* section_name){
    if(!head || !stbl || sh_num <= 0){
        return NULL;
    }

    //read section strtab section
    Elf64_Shdr* str_p = stbl[head->e_shstrndx];
    if(str_p){
        char *str = readsection(fd, str_p);
        for(int i = 0; i<sh_num; i++){
            if(strcmp(section_name, str + stbl[i]->sh_name) == 0){
                return stbl[i];
            }
        }
    }

    return NULL;
}

//read all content of given section
char* readsection(int fd, Elf64_Shdr* sh){
    if(!sh){
        return "";
    }
    char* buff = malloc(sh->sh_size);
    if(!buff) {
        return "";
    }

    if(buff){
        //lseek point first
        if(lseek(fd,(off_t)sh->sh_offset, SEEK_SET) == (off_t)sh->sh_offset){
            if(read(fd, (void *)buff, sh->sh_size) == sh->sh_size){
                return buff;
            }
        }
    }
    return "";
}

int getRPath(int fd, char *rpath){
    if(fd <= 0){
        return -1;
    }

    Elf32_Ehdr h32;
    if(read_elf_header32(fd, &h32) == 0){
        if(validELF(&h32) && validType(&h32)){
            Elf64_Ehdr h64;
            if(read_elf_header64(fd, &h64) == 0){
                Elf64_Shdr **shead = NULL;
                int shnum = getAllSectionHeaders(fd, &h64, &shead);
                if(shnum > 0 && shead){
                    Elf64_Shdr *sdh = findSectionHead(fd, &h64, shead, shnum, ".dynamic");
                    Elf64_Shdr *sdsh = findSectionHead(fd, &h64, shead, shnum, ".dynstr");

                    char *strTab = readsection(fd, sdsh);
                    char *content = readsection(fd, sdh);

                    Elf64_Dyn *dyn = (Elf64_Dyn *)content;
                    Elf64_Dyn *dynRPath = NULL, *dynRunPath = NULL;
                    for(; dyn->d_tag != DT_NULL; dyn++){
                        if (dyn->d_tag == DT_RPATH) {
                            dynRPath = dyn;
                            /* Only use DT_RPATH if there is no DT_RUNPATH. */
                            if (!dynRunPath)
                                sprintf(rpath + strlen(rpath), "%s:", strTab + dyn->d_un.d_val);
                        }
                        else{
                            if(dyn->d_tag == DT_RUNPATH){
                                dynRunPath = dyn;
                                sprintf(rpath + strlen(rpath), "%s:", strTab + dyn->d_un.d_val);
                            }
                        }
                    }

                    if(rpath[strlen(rpath) - 1]== ':'){
                        rpath[strlen(rpath) - 1] = '\0';
                    }

                    //clean up
                    if(shead){
                        for(int i = 0; i<shnum; i++){
                            if(shead[i]){
                                free(shead[i]);
                            }
                        }
                        free(shead);
                        shead = NULL;
                    }

                    if(strTab){
                        free(strTab);
                    }
                    if(content){
                        free(content);
                    }

                    return 0;
                }
            }
        }
    }

    return -1;
}

int getNeedLibs(int fd, char *needed){
    if(fd <= 0){
        return -1;
    }

    Elf32_Ehdr h32;
    if(read_elf_header32(fd, &h32) == 0){
        if(validELF(&h32) && validType(&h32)){
            Elf64_Ehdr h64;
            if(read_elf_header64(fd, &h64) == 0){
                Elf64_Shdr **shead = NULL;
                int shnum = getAllSectionHeaders(fd, &h64, &shead);
                if(shnum > 0 && shead){
                    Elf64_Shdr *sdh = findSectionHead(fd, &h64, shead, shnum, ".dynamic");
                    Elf64_Shdr *sdsh = findSectionHead(fd, &h64, shead, shnum, ".dynstr");

                    char *strTab = readsection(fd, sdsh);
                    char *content = readsection(fd, sdh);

                    Elf64_Dyn *dyn = (Elf64_Dyn *)content;
                    Elf64_Dyn *dynRPath = NULL, *dynRunPath = NULL;
                    for(; dyn->d_tag != DT_NULL; dyn++){
                        if (dyn->d_tag == DT_NEEDED)
                            sprintf(needed + strlen(needed), "%s:", strTab + dyn->d_un.d_val);
                    }

                    if(needed[strlen(needed) - 1]== ':'){
                        needed[strlen(needed) - 1] = '\0';
                    }

                    //clean up
                    if(shead){
                        for(int i = 0; i<shnum; i++){
                            if(shead[i]){
                                free(shead[i]);
                            }
                        }
                        free(shead);
                        shead = NULL;
                    }

                    if(strTab){
                        free(strTab);
                    }
                    if(content){
                        free(content);
                    }
                    return 0;
                }
            }
        }
    }
    return -1;
}

/**
int main(int argc, char *argv[]){
    assert(argc > 1);

    int fd = open(argv[1], O_RDONLY);
    assert(fd);

    char rpath[1024];
    memset(rpath,'\0',1024);
    int rret = getRPath(fd, rpath);
    if(rret == 0){
        printf("rpath: %s\n", rpath);
    }

    char needed[1024];
    memset(needed,'\0',1024);
    int nret = getNeedLibs(fd, needed);
    if(nret == 0){
        printf("libs needed: %s\n", needed);
    }

    if(fd){
        close(fd);
    }
    return 0;
}
**/
