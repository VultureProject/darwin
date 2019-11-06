/* file_utils.h
 *
 * This header contains prototypes for file_utils.c,
 * being functions to manage files
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

#ifndef FILE_UTILS_H
#define FILE_UTILS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/types.h>
#include <stdint.h>
#include <pthread.h>
#include <libgen.h>

#ifdef __gnu_linux__
#include <assert.h>
#endif

typedef struct FileStruct_ {
    char *directory;
    char *filename;
    FILE *pFile;
    pthread_mutex_t mFile;
    uint32_t size;
} FileStruct;

void addDataToFile(char *pData, uint32_t sizeData, uint32_t offSet, FileStruct *file);
void appendLineToFile(char *, FileStruct *);
FILE *openFile(char *path, char *file_name);
int createFolder(char *folder);
FileStruct *createFileStruct();
void deleteFileStruct(FileStruct *);

#ifdef __cplusplus
};
#endif

#endif /* FILE_UTILS_H */
