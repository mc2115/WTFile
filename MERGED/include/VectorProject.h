#ifndef _VECTOR_PROJECT_
#define _VECTOR_PROJECT_

#include "VectorStr.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VECTOR_INITIAL_CAPACITY 32
typedef struct
{
    char project[32];
    pthread_mutex_t lock;
    VectorStr commitFiles;
} Project;

typedef struct
{
    int size;
    int capacity;
    Project *data;
} VectorProject;

void project_init(VectorProject *vector)
{
    vector->size = 0;
    vector->capacity = VECTOR_INITIAL_CAPACITY;
    vector->data = (Project *)malloc(sizeof(Project) * vector->capacity);
}

void project_resize(VectorProject *vector)
{
    if (vector->size >= vector->capacity)
    {
        vector->capacity *= 2;
        vector->data = (Project *)realloc(vector->data, sizeof(Project) * vector->capacity);
    }
}

void project_append(VectorProject *vector, Project proj)
{
    project_resize(vector);
    vector->data[vector->size++] = proj;
}

Project *project_getData(VectorProject *vector, int i)
{
    return &vector->data[i];
}

int project_size(VectorProject *vector)
{
    return vector->size;
}

int project_capacity(VectorProject *vector)
{
    return vector->capacity;
}

unsigned char project_is_empty(VectorProject *vector)
{
    return vector->size == 0;
}

void project_free_memory(VectorProject *vector)
{
    free(vector->data);
}

Project *project_getOrCreate(VectorProject *vector, const char *project)
{
    int idx = 0;
    for (; idx < project_size(vector); idx++)
    {
        if (!strcmp(vector->data[idx].project, project))
        {
            return &vector->data[idx];
        }
    }
    Project proj;
    project_append(vector, proj);
    Project *p = project_getData(vector, project_size(vector) - 1); //get last, the one that just added
    snprintf(p->project, sizeof(p->project), "%s", project);
    vector->size++;
    pthread_mutex_init(&p->lock, NULL);
    str_init(&p->commitFiles);
    return p;
}

int project_addCommitFile(Project *proj, const char *filename)
{
    str_append(&proj->commitFiles, filename);
    return 0;
}

#endif
