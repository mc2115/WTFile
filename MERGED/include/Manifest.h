#ifndef __MANIFEST_HEADER__
#define __MANIFEST_HEADER__

#include "Utils.h"

struct FileManifest
{
    short version;
    char filename[32];
    char hash[65]; //hash in hex string
};

typedef struct FileManifest FileManifest;

#define VECTOR_INITIAL_CAPACITY 32
typedef struct
{
    int size;
    int capacity;
    FileManifest *data;
} VectorFileManifest;

struct ProjectManifest
{
    short version;
    char project[32];
    VectorFileManifest vector;
};

typedef struct ProjectManifest ProjectManifest;

void filemanifest_init(VectorFileManifest *vector)
{
    vector->size = 0;
    vector->capacity = VECTOR_INITIAL_CAPACITY;
    vector->data = (FileManifest *)malloc(sizeof(FileManifest) * vector->capacity);
}

void filemanifest_resize(VectorFileManifest *vector)
{
    if (vector->size >= vector->capacity)
    {
        vector->capacity *= 2;
        vector->data = (FileManifest *)realloc(vector->data, sizeof(FileManifest) * vector->capacity);
    }
}

void filemanifest_append(VectorFileManifest *vector, FileManifest manifest)
{
    filemanifest_resize(vector);
    vector->data[vector->size++] = manifest;
}

FileManifest *filemanifest_getData(VectorFileManifest *vector, int i)
{
    return &vector->data[i];
}

int filemanifest_size(VectorFileManifest *vector)
{
    return vector->size;
}

int filemanifest_capacity(VectorFileManifest *vector)
{
    return vector->capacity;
}

unsigned char filemanifest_is_empty(VectorFileManifest *vector)
{
    return vector->size == 0;
}

FileManifest *filemanifest_findData(VectorFileManifest *vector, const char *filename)
{
    int i = 0;
    for (; i < filemanifest_size(vector); ++i)
    {
        if (!strcmp(vector->data[i].filename, filename))
        {
            return &vector->data[i];
        }
    }
    return NULL;
}

int RemoveFile(VectorFileManifest *vector, const char *filename)
{
    int i = 0;
    for (; i < filemanifest_size(vector); i++)
    {
        if (!strcmp(vector->data[i].filename, filename))
        {
            int j = i;
            for (; j < filemanifest_size(vector) - 1; j++)
            {
                //vector->data[j] = vector->data[j+1];
                memcpy(&vector->data[j], &vector->data[j + 1], sizeof(FileManifest));
            }
            vector->size--;
            return 0;
        }
    }
    return -1;
}

void Free_memory(VectorFileManifest *vector)
{
    free(vector->data);
}

FileManifest *findFileManifest(ProjectManifest *pm, const char *filename)
{
    if (pm == NULL)
    {
        return NULL;
    }

    return filemanifest_findData(&pm->vector, filename);
}

int addFileManifest(ProjectManifest *pm, const char *filename)
{
    FileManifest manifest;
    memset(&manifest, '\0', sizeof(manifest));
    manifest.version = 1;
    memset(manifest.filename, '\0', sizeof(manifest.filename));
    snprintf(manifest.filename, sizeof(manifest.filename), "%s", filename);
    char fullname[128];
    fullFilename(fullname, pm->project, filename);
    if (calcDigest(manifest.hash, fullname) < 0)
    {
        return -1;
    }
    filemanifest_append(&pm->vector, manifest);
    return 0;
}

int addFileManifestWithHash(ProjectManifest *pm, const char *filename, char *hash)
{
    FileManifest manifest;
    memset(&manifest, '\0', sizeof(manifest));
    manifest.version = 1;
    memset(manifest.filename, '\0', sizeof(manifest.filename));
    snprintf(manifest.filename, sizeof(manifest.filename), "%s", filename);
    snprintf(manifest.hash, sizeof(manifest.hash), "%s", hash);
    filemanifest_append(&pm->vector, manifest);
    return 0;
}

int updateFileManifestWithHash(ProjectManifest *pm, const char *filename, char *hash)
{
    int idx = 0;
    for (; idx < filemanifest_size(&pm->vector); idx++)
    {
        if (!strcmp(pm->vector.data[idx].filename, filename))
        {
            strncpy(pm->vector.data[idx].hash, hash, strlen(hash));
            pm->vector.data[idx].hash[64] = '\0';
            pm->vector.data[idx].version++;
            break;
        }
    }
    return 0;
}

int persistProjectManifest(ProjectManifest *pm)
{
    char manifestFilename[128];
    fullFilename(manifestFilename, pm->project, ".Manifest");
    FILE *mf = fopen(manifestFilename, "w");
    if (mf == NULL)
    {
        reportError("Failed to open manifest file %s for write", manifestFilename);
        return -1;
    }
    fprintf(mf, "%d %s 0000\n", pm->version, pm->project);
    int i = 0;
    for (; i < filemanifest_size(&(pm->vector)); ++i)
    {
        FileManifest *m = filemanifest_getData(&pm->vector, i);
        fprintf(mf, "%d %s %s\n", m->version, m->filename, m->hash);
    }
    fclose(mf);
    free(pm);
    return 0;
}

int removeFileManifest(ProjectManifest *pm, const char *filename)
{
    if (RemoveFile(&(pm->vector), filename) < 0)
    {
        reportError("File %s is not in project %s", filename, pm->project);
        return -1;
    }
    return 0;
}

ProjectManifest *initProjectManifest(const char *project)
{
    char manifestFilename[128];
    fullFilename(manifestFilename, project, ".Manifest");
    FILE *mf = fopen(manifestFilename, "r");
    if (mf == NULL)
    {
        reportError("Failed to open manifest file %s for reading", manifestFilename);
        return NULL;
    }
    ProjectManifest *pm = (ProjectManifest *)malloc(sizeof(ProjectManifest));
    char dummy[32];
    fscanf(mf, "%hd%s%s\n", &pm->version, pm->project, dummy);
    //memset(pm->project, '\0', sizeof(pm->project));
    //strncpy(pm->project, project, strlen(project));
    FileManifest manifest;
    filemanifest_init(&pm->vector);
    while (3 == fscanf(mf, "%hd%s%s\n", &manifest.version, manifest.filename, manifest.hash))
    {
        filemanifest_append(&pm->vector, manifest);
    }
    fclose(mf);
    return pm;
}

//compare server manifest and client manifest
int updateFromManifest(const char *smName, const char *cmName)
{
    ProjectManifest *serverPM = initProjectManifest("./");
    if (serverPM == NULL)
    {
        return -1;
    }
    ProjectManifest *clientPM = initProjectManifest(cmName);
    if (clientPM == NULL)
    {
        return -1;
    }

    FILE *update = NULL;
    FILE *conflict = NULL;

    if (serverPM->version == clientPM->version)
    {
        openSpecialFile(&update, cmName, ".Update");
        fclose(update);
        reportInfo("server and client have the same version. there is nothing to update");
        return 0;
    }
    reportInfo("Number of files in the manifest server:%d client:%d", filemanifest_size(&serverPM->vector), filemanifest_size(&clientPM->vector));
    //sort filepm for effiency
    //Iterate through files from its manifest file
    int i = 0;
    for (; i < filemanifest_size(&serverPM->vector); i++)
    {
        FileManifest *serverFM = filemanifest_getData(&serverPM->vector, i);
        FileManifest *clientFM = filemanifest_findData(&clientPM->vector, serverFM->filename);
        if (clientFM == NULL)
        {
            char livehash[65];
            openSpecialFile(&update, cmName, ".Update");
            fprintf(update, "A %s %s\n", serverFM->filename, serverFM->hash);
            reportInfo("A %s/%s", clientPM->project, serverFM->filename);
        }
        else
        {
            if (strcmp(clientFM->hash, serverFM->hash) != 0)
            {
                char livehash[65];
                char fullname[128];
                fullFilename(fullname, clientPM->project, clientFM->filename);
                calcDigest(livehash, fullname);
                if (strcmp(livehash, clientFM->hash) == 0)
                {
                    openSpecialFile(&update, cmName, ".Update");
                    fprintf(update, "M %s %s\n", clientFM->filename, serverFM->hash);
                    reportInfo("M %s", clientFM->filename);
                }
                else
                {
                    openSpecialFile(&conflict, cmName, ".Conflict");
                    fprintf(conflict, "C %s %s\n", clientFM->filename, livehash);
                    reportInfo("C %s", clientFM->filename);
                }
            }
        }
    }
    //Iterate through all file in client's manifest
    i = 0;
    for (; i < filemanifest_size(&clientPM->vector); i++)
    {
        FileManifest *clientFM = filemanifest_getData(&clientPM->vector, i);
        FileManifest *serverFM = filemanifest_findData(&serverPM->vector, clientFM->filename);
        //printf("Filename:%s\n",clientFM->filename);
        if (serverFM == NULL)
        {
            openSpecialFile(&update, cmName, ".Update");
            fprintf(update, "D %s %s\n", clientFM->filename, clientFM->hash);
            reportInfo("D %s/%s", clientPM->project, clientFM->filename);
        }
    }
    if (serverPM->version != clientPM->version)
    {
        openSpecialFile(&update, cmName, ".Update");
        fprintf(update, "V %d %s\n", serverPM->version, "dummy");
        //Just to save the version of the server manifest, which will be used to upgrade the client version
        //The 3rd argument is useless, just to make it consistent to make fscanf in upgradeProject() work
        fclose(update);
    }
    if (conflict != NULL)
        fclose(conflict);
    return 0;
}
//compare server manifest and client manifest
int compare2Manifest(const char *smName, const char *cmName)
{
    ProjectManifest *serverPM = initProjectManifest("./");
    if (serverPM == NULL)
    {
        return -1;
    }
    ProjectManifest *clientPM = initProjectManifest(cmName);
    if (clientPM == NULL)
    {
        return -1;
    }
    if (serverPM->version != clientPM->version)
    {
        reportError("server manifest version %d != client manifest version %d, please run update first",
                    serverPM->version, clientPM->version);
        return -1;
    }

    FILE *commit;

    reportInfo("Number of files on server:%d on client:%d", filemanifest_size(&serverPM->vector), filemanifest_size(&clientPM->vector));
    //sort filepm for effiency
    //Iterate through files from its manifest file
    int i = 0;
    for (; i < filemanifest_size(&serverPM->vector); i++)
    {
        FileManifest *serverFM = filemanifest_getData(&serverPM->vector, i);
        FileManifest *clientFM = filemanifest_findData(&clientPM->vector, serverFM->filename);
        if (clientFM == NULL)
        {
            openSpecialFile(&commit, cmName, ".Commit");
            char livehash[65];
            fprintf(commit, "D %s\n", serverFM->filename);
            reportInfo("D %s/::::%s", clientPM->project, serverFM->filename);
        }
        else
        {
            if (strcmp(clientFM->hash, serverFM->hash) != 0)
            {
                if (serverFM->version >= clientFM->version)
                {
                    reportError("File %s: server's version:%d newer than client's version:%d, please run update first HASH:%s:%s",
                                clientFM->filename, serverFM->version, clientFM->version, clientFM->hash, serverFM->hash);
                    if (commit != NULL)
                    {
                        fclose(commit);
                        char fullname[128];
                        fullFilename(fullname, clientPM->project, ".Commit");
                        unlink(fullname);
                    }
                    return -1;
                }
            }
            else
            {
                char livehash[65];
                char fullname[128];
                fullFilename(fullname, clientPM->project, clientFM->filename);
                calcDigest(livehash, fullname);
                if (strcmp(livehash, clientFM->hash) != 0)
                {
                    openSpecialFile(&commit, cmName, ".Commit");
                    fprintf(commit, "M %s %s\n", clientFM->filename, livehash);
                    reportInfo("M %s/%s", clientPM->project, clientFM->filename);
                }
            }
        }
    }
    //Iterate through all file in client's manifest
    i = 0;
    for (; i < filemanifest_size(&clientPM->vector); i++)
    {
        FileManifest *clientFM = filemanifest_getData(&clientPM->vector, i);
        FileManifest *serverFM = filemanifest_findData(&serverPM->vector, clientFM->filename);
        if (serverFM == NULL)
        {
            char livehash[65];
            char fullname[128];
            fullFilename(fullname, clientPM->project, clientFM->filename);
            calcDigest(livehash, fullname);
            openSpecialFile(&commit, cmName, ".Commit");
            fprintf(commit, "A %s %s\n", clientFM->filename, livehash);
            reportInfo("A %s/%s", clientPM->project, clientFM->filename);
        }
    }
    if (commit != NULL)
    {
        fclose(commit);
    }
    persistProjectManifest(clientPM);
    return 0;
}

int updatePMWithFile(ProjectManifest *pm, FileManifest *fm)
{
    int i = 0;
    for (; i < filemanifest_size(&pm->vector); i++)
    {
        if (!strcmp(pm->vector.data[i].filename, fm->filename))
        {
            pm->vector.data[i].version++;
            strncpy(pm->vector.data[i].hash, fm->hash, strlen(fm->hash));
            return 0;
        }
    }
    fm->version = 1;
    filemanifest_append(&pm->vector, *fm);
    return 0;
}

int displayCurrentversion(const char *project)
{
    ProjectManifest *pm = initProjectManifest("./");
    if (pm == NULL)
    {
        return -1;
    }
    reportInfo("Project:%s Version:%d", project, pm->version);
    int i = 0;
    for (; i < filemanifest_size(&pm->vector); i++)
    {
        reportInfo("%d %s", pm->vector.data[i].version, pm->vector.data[i].filename);
    }
    return 0;
}

#endif
