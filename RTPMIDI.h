/* Copyright (c) 2021 ARM Limited
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef RTPMIDI_H
#define RTPMIDI_H

#include <stdint.h>
#include "NetworkInterface.h"
#include "UDPSocket.h"

#if BYTE_ORDER == LITTLE_ENDIAN
#define htonll(x) ((((uint64_t)lwip_htonl(x)) << 32) + lwip_htonl((x) >> 32)) 
#endif /* BYTE_ORDER == LITTLE_ENDIAN */

#define CONTROL_PORT    5004
#define MIDI_PORT       5005

#define MAX_NAME_LENGTH 32

#define EXCHANGE_SIGNATURE   0xFFFF
#define INV_COMMAND          0x494E
#define ACCEPT_INV_COMMAND   0x4F4B
#define REJECT_INV_COMMAND   0x4E4F
#define END_COMMAND          0x4259
#define PROTOCOL_VERSION     2

#define SYNC_CK0    0
#define SYNC_CK1    1
#define SYNC_CK2    2

/* TODO: Generate these somehow, rather than defining them */
#define SSRC_NUMBER          0x2d5d5ff3
#define NAME                 "k66f"

/* Session Exchange Packet */
typedef struct {
    uint16_t signature;
    uint16_t command;
    uint32_t protocol_version;
    uint32_t initiator_token;
    uint32_t sender_ssrc;
    char name[MAX_NAME_LENGTH];
} __packed exchange_packet;

/* Timestamp Packet */
typedef struct {
    uint16_t signature;
    uint16_t command;
    uint32_t sender_ssrc;
    uint8_t count;
    uint8_t padding[3];
    uint64_t timestamp[3];
} __packed timestamp_packet;

class RTPMIDI {
public:

    /**
    * Basic Constructor using default NetworkInterface
    */
    RTPMIDI();

    /**
    * Constructor specifying NetworkInterface
    */
    RTPMIDI(NetworkInterface *net);

    /**
    * Destroy this object
    */
    ~RTPMIDI();

    /**
    * Begin session initiation as participant
    */
    void participate();

private:
    NetworkInterface *_net;
    UDPSocket _control_socket;
    UDPSocket _midi_socket;

    uint64_t _calculate_current_timestamp();
};

#endif
