/* rand_utils.h
 *
 *  This file contains functions related to random numbers generation
 *
 * File begun on 2019-17-5
 *
 * Created by:
 *  - Théo Bertin (theo.bertin@advens.fr)
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
#ifdef __cplusplus
extern "C" {
#endif

#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef RAND_UTILS_H
#define RAND_UTILS_H

long int getRandom();

#ifdef __cplusplus
};
#endif

#endif /* RAND_UTILS_H */
