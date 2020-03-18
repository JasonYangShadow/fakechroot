#include <stdbool.h>

typedef struct queue{
    char *val;
    struct queue *next;
}Queue;

bool QueuePush(Queue **, const char *);
bool QueuePop(Queue **, char *);
bool CleanQueue(Queue **);
