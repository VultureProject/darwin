/* packets-utils.h
 *
 * This header contains the definition of Packet utilities
 *
 * File begun on 2018-21-5
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

#ifndef PACKET_UTILS_H
#define PACKET_UTILS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/param.h>

#ifdef __FreeBSD__
#include <netinet/in.h>
#include <sys/socket.h>
#endif
#include <arpa/inet.h>

/* FlowHash is just an uint32_t */
typedef uint32_t FlowHash;

/* Port is just a uint16_t */
typedef uint16_t Port;
#define CMP_PORT(p1, p2) \
    ((p1) == (p2))

/* Address */
typedef struct Address_ {
    char family;
    union {
        uint32_t        address_un_data32[4]; /* type-specific field */
        uint16_t        address_un_data16[8]; /* type-specific field */
        uint8_t         address_un_data8[16]; /* type-specific field */
        struct in6_addr address_un_in6;
    } address;
} Address;

#define addr_data32 address.address_un_data32
#define addr_data16 address.address_un_data16
#define addr_data8  address.address_un_data8
#define addr_in6addr    address.address_un_in6

#define COPY_ADDR(a, b) do {                    \
        (b)->family = (a)->family;                 \
        (b)->addr_data32[0] = (a)->addr_data32[0]; \
        (b)->addr_data32[1] = (a)->addr_data32[1]; \
        (b)->addr_data32[2] = (a)->addr_data32[2]; \
        (b)->addr_data32[3] = (a)->addr_data32[3]; \
    } while (0)

/* clear the address structure by setting all fields to 0 */
#define CLEAR_ADDR(a) do {       \
        (a)->family = 0;         \
        (a)->addr_data32[0] = 0; \
        (a)->addr_data32[1] = 0; \
        (a)->addr_data32[2] = 0; \
        (a)->addr_data32[3] = 0; \
    } while (0)

#define CMP_ADDR(a1, a2) \
    (((a1)->addr_data32[3] == (a2)->addr_data32[3] && \
      (a1)->addr_data32[2] == (a2)->addr_data32[2] && \
      (a1)->addr_data32[1] == (a2)->addr_data32[1] && \
      (a1)->addr_data32[0] == (a2)->addr_data32[0]))

static inline char *getAddrString(Address addr) {
    char *ret = (char *)malloc(50);
    if(addr.family == AF_INET) {
        if(!inet_ntop(AF_INET, (const void *)&(addr.address), ret, 50)) {
            strncpy(ret, "unknown", 50);
        }
    }
    else {
        if(!inet_ntop(AF_INET6, (const void*)&(addr.addr_in6addr), ret, 50)) {
            strncpy(ret, "unknown", 50);
        }
    }

    return ret;
}

#ifdef __cplusplus
};
#endif

#endif /* PACKET_UTILS_H */
