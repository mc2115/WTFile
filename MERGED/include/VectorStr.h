#ifndef _VECTOR_STR_
#define _VECTOR_STR_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VECTOR_INITIAL_CAPACITY 32
typedef struct
{
    int size;
    int capacity;
    char **data;
} VectorStr;

void str_init(VectorStr *vector)
{
    vector->size = 0;
    vector->capacity = VECTOR_INITIAL_CAPACITY;
    vector->data = (char **)malloc(sizeof(char *) * vector->capacity);
}

void str_reset(VectorStr *vector)
{
    vector->size = 0;
}

void str_resize(VectorStr *vector)
{
    if (vector->size >= vector->capacity)
    {
        vector->capacity *= 2;
        vector->data = (char **)realloc(vector->data, sizeof(char *) * vector->capacity);
    }
}

void str_append(VectorStr *vector, const char *str)
{
    str_resize(vector);
    vector->data[vector->size++] = strdup(str);
}

char *str_getData(VectorStr *vector, int i)
{
    return vector->data[i];
}

int str_size(VectorStr *vector)
{
    return vector->size;
}

int str_capacity(VectorStr *vector)
{
    return vector->capacity;
}

unsigned char str_is_empty(VectorStr *vector)
{
    return vector->size == 0;
}

void str_free_memory(VectorStr *vector)
{
    int i = 0;
    for (; i < vector->size; i++)
    {
        free(vector->data[i]);
    }
    free(vector->data);
}
#endif
