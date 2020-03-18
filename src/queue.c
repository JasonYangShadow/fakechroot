#include "queue.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#define PATH_MAX 1024

bool QueuePush(Queue **q, const char *val){
    if(val && *val){
        Queue *nq = (Queue *)malloc(sizeof(Queue));
        nq->val = (char *)malloc(PATH_MAX);
        strcpy(nq->val, val);
        nq->next = NULL;

        if(*q){
            Queue *cur = *q;
            while(cur->next){
                cur = cur->next;
            }
            cur->next = nq;
        }else{
            *q = nq;
        }

        return true;
    }

    return false;
}

bool QueuePop(Queue **q, char *val){
    if(*q && (*q)->val){
        strcpy(val, (*q)->val);
        Queue *next = (*q)->next;
        free((*q)->val);
        free(*q);
        *q = next;
        return true;
    }
    return false;
}

bool CleanQueue(Queue **q){
    while(*q){
        Queue *next = (*q)->next;
        if((*q)->val){
            free((*q)->val);
            free(*q);
        }
        *q = next;
    }
    return true;
}
