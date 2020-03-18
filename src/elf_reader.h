#include <string.h>
#include <sys/mman.h>
#include <elf.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>

bool validELF(Elf32_Ehdr *hd);
bool validType(Elf32_Ehdr *hd);
Elf64_Shdr* findSectionHead(int fd, Elf64_Ehdr *head, Elf64_Shdr** stbl, int sh_num, char* section_name);
int getAllSectionHeaders(int fd, Elf64_Ehdr *head, Elf64_Shdr ***shead);
char* readsection(int fd, Elf64_Shdr* sh);
int getNeedLibs(int fd, char *needed);
int getRPath(int fd, char *rpath);
