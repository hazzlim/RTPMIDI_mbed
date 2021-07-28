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
#include <deque>
#include "NetworkInterface.h"
#include "UDPSocket.h"

// TODO: Include this header without having to include mbed-usb
#include "MIDIMessage.h"

#if BYTE_ORDER == LITTLE_ENDIAN
#define htonll(x) ((((uint64_t)lwip_htonl(x)) << 32) + lwip_htonl((x) >> 32)) 
#endif /* BYTE_ORDER == LITTLE_ENDIAN */

#define CONTROL_PORT    5004
#define MIDI_PORT       5005

#define MAX_NAME_LENGTH 32

// TODO: something that makes more sense than this
#define MAX_BUFFER_SIZE MAX_MIDI_MESSAGE_SIZE

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

/* MIDI Packet Header */
typedef struct {
    uint8_t vpxcc;
    uint8_t mpayload;
    uint16_t sequence_number;
    uint32_t timestamp;
    uint32_t sender_ssrc;
} __packed midi_packet_header;

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

    /**
    * Send a MIDIMessage
    *
    * @param msg The MIDIMessage to send
    */
    void write(MIDIMessage msg);

private:
    NetworkInterface *_net;
    UDPSocket _control_socket;
    UDPSocket _midi_socket;
    std::deque<uint8_t> _out_midi_buffer;

    uint64_t _calculate_current_timestamp();
    void _send_midi_buffer();
};

#endif
