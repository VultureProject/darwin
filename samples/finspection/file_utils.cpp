/* file_utils.c
 *
 *  This file contains functions related to files and folders (creation, modification...)
 *
 * File begun on 2018-12-5
 *
 * Created by:
 *  - François Bernard (francois.bernard@isen.yncrea.fr)
 *  - Théo Bertin (theo.bertin@isen.yncrea.fr)
 *  - Tianyu Geng (tianyu.geng@isen.yncrea.fr)
 *
 * This file is part of rsyslog.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *       -or-
 *       see COPYING.ASL20 in the source distribution
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "file_utils.hpp"
#include "Logger.hpp"

/*
 *  This method write an amount of bytes at a specific offset in an open file
 *
 *  It gets in parameters:
 *  - a pointer on the data
 *  - the number of bytes to write
 *  - the offset where to begin writing (beginning at zero)
 *  - the file structure where the data will be written
 *
 *  The file structure must not be NULL and must already be created
 *
 *  If the offset provided is past the end of the file, zeros will be written
 *  to fill the missing space
*/
void addDataToFile(char* pData, uint32_t sizeData, uint32_t offSet, FileStruct* file){
    char zero = 0x00;
    uint32_t i;

    pthread_mutex_lock(&(file->mFile));

    fseek(file->pFile, 0, SEEK_END);
    int diff = offSet - ftell(file->pFile);
    if (diff > 0){
        for(i = 0; i < (uint32_t)diff; i++){
            fwrite(&zero, sizeof(char), 1, file->pFile);
        }
    }

    fseek(file->pFile, offSet, SEEK_SET);
    fwrite(pData, sizeof(char), sizeData, file->pFile);
    fflush(file->pFile);

    file->size += sizeData;

    pthread_mutex_unlock(&(file->mFile));
    return;
}

void appendLineToFile(char *pLine, FileStruct *file) {
    int lineLength = strlen(pLine);

    pthread_mutex_lock(&(file->mFile));

    fseek(file->pFile, 0, SEEK_END);
    fwrite(strcat(pLine, "\r\n"), sizeof(char), lineLength + 2, file->pFile);
    fflush(file->pFile);

    file->size += lineLength + 2;

    pthread_mutex_unlock(&(file->mFile));
    return;
}

/*
 *  This function opens a file given a name and a path:
 *  - if the file exists it is simply opened
 *  - if the file doesn't exists, it is created in 'path'
 *
 *  The parameters are:
 *  - a char array representing the (complete) path to the
 *      folder containing the file
 *  - a char array representing the name of the file
 *
 *  The returned value is either the link on the opened file,
 *  or NULL if an error occured
 *  There is no error code returned, but simply a debug message
*/
FILE* openFile(char* path, char* file_name){
    DIR *dir;
    FILE *file = NULL;
    char *new_file_comp_path = NULL;
    char *pChar;

    assert(file_name != NULL);
    assert(path != NULL);

    pChar = path;
    while(*pChar++ != '\0') { }
    if(*--pChar == '/') *pChar = '\0';

    dir = opendir(path);
    if(!dir) {
        createFolder(path);
        dir = opendir(path);
    }

    if(dir != NULL){
        new_file_comp_path = (char *)malloc(strlen(path)+1+strlen(file_name)+1);
        strcpy(new_file_comp_path,path);
        strcat(new_file_comp_path,"/");
        strcat(new_file_comp_path,file_name);

        /* assuming file is created, opening */
        file = fopen(new_file_comp_path,"r+");

        if(file == NULL){
            /* creating file */
            file = fopen(new_file_comp_path, "w");
        }
    }

    closedir(dir);
    if(new_file_comp_path) free(new_file_comp_path);
    return file;
}

/*
 *  This function creates a folder given its complete path+name
 *
 *  It gets a char array representing the complete path
 *  AND the name of the folder
 *
 *  It returns zero if the folder was created successfully,
 *  a negative value otherwise
 *
 *  It cannot create parent folders if they do not already exist,
 *  separate calls are necessary to create them
*/
int createFolder(char* folder){
    DARWIN_LOGGER;
    struct stat file_stat;
    int ret;

    assert(folder != NULL);

    ret = stat(folder, &file_stat);
    if(ret<0)
    {
        if(errno == ENOENT)
        {
            ret = mkdir(folder, 0775);

            if(ret == -1) {
                switch(errno) {
                    case EACCES:
                            DARWIN_LOG_ERROR("FILE_UTILS::createFolder:: cannot create folder, access denied");
                            break;
                    case EEXIST:
                            DARWIN_LOG_ERROR("FILE_UTILS::createFolder:: tried creating existing folder");
                            return 0;
                    case ENAMETOOLONG:
                            DARWIN_LOG_ERROR("FILE_UTILS::createFolder:: cannot create folder, name is too long");
                            break;
                    case ENOENT:
                            createFolder(dirname(folder));
                            break;
                    case ENOSPC:
                            DARWIN_LOG_ERROR("FILE_UTILS::createFolder:: cannot create folder, no space left on disk");
                            break;
                    case EROFS:
                            DARWIN_LOG_ERROR("FILE_UTILS::createFolder:: cannot create folder, read-only filesystem");
                            break;
                }
                return ret;
            }
        }
    }
    return 0;
}

FileStruct *createFileStruct() {
    FileStruct *newFile = (FileStruct *)calloc(1, sizeof(FileStruct));
    pthread_mutex_init(&(newFile->mFile), NULL);
    newFile->directory = (char *)malloc(2048);
    newFile->filename = (char *)malloc(256);
    return newFile;
}

void deleteFileStruct(FileStruct *file) {
    if(file) {
        pthread_mutex_destroy(&(file->mFile));
        free(file->directory);
        free(file->filename);
        if(file->pFile) fclose(file->pFile);
        free(file);
    }
    return;
}