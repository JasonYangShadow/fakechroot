#include "stack.h"
#include <stdlib.h>
#include <string.h>
#define PATH_MAX 1024

bool InitializeStack(size_t max_size, Stack **st){
    if(*st){
        return false;
    }
    *st = (Stack *)malloc(sizeof(Stack));
    if(!(*st)){
        return false;
    }
    (*st)->max_size = max_size;
    (*st)->size = 0;
    (*st)->array = (char **)malloc(sizeof(char *)*max_size);
    if(!((*st)->array)){
        return false;
    }
    return true;
}

bool StackPop(Stack *st, char *str){
    if(!st){
        return false;
    }

    if(st->size <= 0){
        return false;
    }

    st->size--;
    memcpy(str, st->array[st->size],PATH_MAX);
    free(st->array[st->size]);
    st->array[st->size] = NULL;
    return true;
}

bool StackPush(Stack *st, char *str){
    if(!str){
        return false;
    }
    if(!st){
        return false;
    }

    if(st->size >= st->max_size -1){
        return false;
    }

    st->array[st->size] = (char *)malloc(PATH_MAX);
    memcpy(st->array[st->size], str, PATH_MAX);
    st->size++;
    return true;
}

//eliminate duplicated items
bool StackPushUnique(Stack *st, char *str){
    if(!str){
        return false;
    }
    if(!st){
        return false;
    }

    if(st->size >= st->max_size -1){
        return false;
    }

    for(int i = 0; i<st->size; i++){
        if(strcmp(st->array[i], str) == 0){
            return true;
        }
    }

    st->array[st->size] = (char *)malloc(PATH_MAX);
    memcpy(st->array[st->size], str, PATH_MAX);
    st->size++;
    return true;
}

bool StackTop(Stack *st, char *str){
    if(!st){
        return false;
    }
    if(st->size <= 0){
        return false;
    }

    int idx = st->size - 1;
    memcpy(str, st->array[idx], PATH_MAX);
    return true;
}

bool CleanStack(Stack **st){
    if(!st || !(*st)){
        return true;
    }

    for(int i = 0; i<(*st)->size - 1; i++){
        if((*st)->array[i]){
            free((*st)->array[i]);
        }
    }
    if(st){
        free(st);
        st = NULL;
    }
    return true;
}

size_t StackSize(Stack *st){
    if(!st){
        return 0;
    }
    return st->size;
}
