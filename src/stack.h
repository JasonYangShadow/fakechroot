#include <stdbool.h>
#include <stddef.h>
#define STACK_DEFAULT_MAX_SIZE 128

typedef struct stack{
    size_t max_size;
    size_t size;
    char **array;
}Stack;

bool InitializeStack(size_t, Stack **);
bool StackPop(Stack *, char *);
bool StackPush(Stack *, char *);
bool StackPushUnique(Stack *, char *);
bool StackTop(Stack *, char *);
bool CleanStack(Stack **st);
size_t StackSize(Stack *);

//cross file vars
Stack *st;
