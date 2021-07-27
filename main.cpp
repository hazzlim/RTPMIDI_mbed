/* mbed Microcontroller Library
 * Copyright (c) 2021 ARM Limited
 * SPDX-License-Identifier: Apache-2.0
 */

#include "mbed.h"
#include "fxos8700cq.h"
#include "EthernetInterface.h"
#include "LWIPStack.h"

#define I2C0_SDA PTD9
#define I2C0_SCL PTD8

/* Accelerometer Interface */
FXOS8700CQ acc(I2C0_SDA, I2C0_SCL);

/* Network Interface */
EthernetInterface net;

/* APPLEMIDI SIP Packet */
typedef struct {
    uint8_t pad1;
    uint8_t pad2;
    uint8_t commandH;
    uint8_t commandL;
    uint32_t protocol_version;
    uint32_t initiator_token;
    uint32_t sender_ssrc;
    char name[20];
} apple_midi_packet;

/* APPLEMIDI Timestamp Packet */
typedef struct {
    uint8_t pad1;
    uint8_t pad2;
    uint8_t c;
    uint8_t k;
    uint32_t sender_SSRC;
    uint8_t count;
    uint8_t unusedH;
    uint16_t unusedL;
    uint64_t timestamp1;
    uint32_t timestamp2H;
    uint32_t timestamp2L;
    uint64_t timestamp3;
} __packed timestamp_packet;

/* MIDI Message Packet */
typedef struct {
    uint16_t vpxccmpt;
    uint16_t seq;
    uint32_t timestamp;
    uint32_t sender_ssrc;
    uint8_t midi_header;
    uint8_t midi1;
    uint8_t midi2;
    uint8_t midi3;
} __packed message_packet;


int modulate(Data &values)
{
    return (0x3FFF * (values.ax + 1)) / 2;
}

int main()
{
    uint32_t ssrc = 0x2d5d5ff3;

    SocketAddress sockAddr;
    // Bring up the ethernet interface
    printf("UDP Socket example\n");
    if (0 != net.connect()) {
        printf("Error connecting\n");
        return -1;
    }

    // Show the network address
    net.get_ip_address(&sockAddr);
    printf("IP address is: %s\n", sockAddr.get_ip_address() ? sockAddr.get_ip_address() : "No IP");

    UDPSocket command;
    UDPSocket midi;
    command.open(&net);
    midi.open(&net);

    // Bind address to control socket
    command.bind(5004);
    apple_midi_packet in_data;
    command.recvfrom(&sockAddr, &in_data, sizeof(apple_midi_packet));

    // send ok
    apple_midi_packet out_data = {
         0xff,
         0xff,
         0x4f,
         0x4b,
         in_data.protocol_version,
         in_data.initiator_token,
         0x2d5d5ff3,
         "k66f"
    };
    sockAddr.set_ip_address("192.168.68.117");
    sockAddr.set_port(5004);
    command.sendto(sockAddr, &out_data, sizeof(apple_midi_packet));

    // Bind address to MIDI socket
    midi.bind(5005);
    midi.recvfrom(&sockAddr, &in_data, sizeof(apple_midi_packet));

    // send ok
    sockAddr.set_ip_address("192.168.68.117");
    sockAddr.set_port(5005);
    midi.sendto(sockAddr, &out_data, sizeof(apple_midi_packet));

    // receive sync message
    timestamp_packet in_time;
    midi.recvfrom(&sockAddr, &in_time, sizeof(timestamp_packet));

    auto start = Kernel::Clock::now();
    uint32_t timestamp2 = (Kernel::Clock::now() - start).count() / 10;
    timestamp2 = lwip_htonl(timestamp2);

    // send timestamp
    timestamp_packet out_time = {
        0xff,
        0xff,
        0x43,
        0x4b,
        0x2d5d5ff3,
        1,
        0,
        0,
        in_time.timestamp1,
        0,
        timestamp2,
        0,
    };
    printf("Time: %d\n", timestamp2);
    midi.sendto(sockAddr, &out_time, sizeof(timestamp_packet));

    // receive sync message
    midi.recvfrom(&sockAddr, &in_time, sizeof(timestamp_packet));

    message_packet msg = {
        0x6180,
        0x45fd,
        timestamp2,
        ssrc,
        0x03,
        0x90,
        0x43,
        0x7f
    };

    acc.init();

    while (1) {
        // sync
        midi.recvfrom(&sockAddr, &in_time, sizeof(timestamp_packet));
        out_time.timestamp1 = in_time.timestamp1;
        timestamp2 = (Kernel::Clock::now() - start).count() / 100;
        timestamp2 = lwip_htonl(timestamp2);
        out_time.timestamp2L = timestamp2;
        midi.sendto(sockAddr, &out_time, sizeof(timestamp_packet));
        // receive sync message
        midi.recvfrom(&sockAddr, &in_time, sizeof(timestamp_packet));

        // send noteon
        msg.seq += 0x100;
        msg.midi1 = 0x90;
        msg.timestamp = (Kernel::Clock::now() - start).count() / 100;
        msg.timestamp = lwip_htonl(msg.timestamp);
        midi.sendto(sockAddr, &msg, sizeof(message_packet));

        ThisThread::sleep_for(100ms);

        // send noteoff
        msg.seq += 0x100;
        msg.midi1 = 0x80;
        msg.timestamp = (Kernel::Clock::now() - start).count() / 100;
        msg.timestamp = lwip_htonl(msg.timestamp);
        midi.sendto(sockAddr, &msg, sizeof(message_packet));

        // poll the sensor and get the values, storing in a struct
        Data values = acc.get_values();

        // generate the bend value from the sensor values
        int bend = modulate(values);

    }
}
