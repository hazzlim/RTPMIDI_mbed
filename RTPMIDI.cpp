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

#include "RTPMIDI.h"
#include "LWIPStack.h"
#include "rtos.h"
#include <chrono>

using namespace std::chrono_literals;

RTPMIDI::RTPMIDI()
    : _net(NetworkInterface::get_default_instance()), _rtp_queue(32 * EVENTS_EVENT_SIZE)
{
    _init();
}

RTPMIDI::RTPMIDI(NetworkInterface *net) : _net(net)
{
    _init();
}

RTPMIDI::~RTPMIDI()
{
    if (_net) {
        _net->disconnect();
    }
}

void RTPMIDI::participate()
{
    if (!_net) {
        printf("Error! No network interface found.\r\n");
        return;
    }

    nsapi_size_or_error_t result = _net->connect();
    if (result !=0) {
        printf("Error! _net->connect() returned: %d\r\n", result);
        return;
    }

    // Show the network address
    SocketAddress sockAddr;
    _net->get_ip_address(&sockAddr);
    printf("IP address is: %s\n", sockAddr.get_ip_address() ? sockAddr.get_ip_address() : "No IP");

    /* open sockets */
    _control_socket.open(_net);
    _midi_socket.open(_net);

    /* bind sockets to ports */
    _control_socket.bind(CONTROL_PORT);
    _midi_socket.bind(MIDI_PORT);

    /* Try to receive Invitation Packet on control port */
    printf("Waiting for invitation...\r\n");

    exchange_packet setupInvitation;
    _control_socket.recvfrom(&_peer_address, &setupInvitation, sizeof(exchange_packet));
    if (setupInvitation.command != lwip_htons(INV_COMMAND)) {
        printf("Error! Received: '%d' command, not 'IN' command.\r\n", setupInvitation.command);
        return;
    }

    printf("invitation received from %s:%d... sending response... \r\n", _peer_address.get_ip_address(), _peer_address.get_port());

    /* Send response packet on control port */
    exchange_packet setupResponse = {
        EXCHANGE_SIGNATURE,
        lwip_htons(ACCEPT_INV_COMMAND),
        lwip_htonl(PROTOCOL_VERSION),
        setupInvitation.initiator_token,
        SSRC_NUMBER,
        NAME
    };
    _control_socket.sendto(_peer_address, &setupResponse, sizeof(exchange_packet));

    printf("Response sent \r\n");

    /* Try to receive Invitation Packet on midi port */
    printf("Waiting for invitation...\r\n");

    _midi_socket.recvfrom(&_peer_address, &setupInvitation, sizeof(exchange_packet));
    if (setupInvitation.command != lwip_htons(INV_COMMAND)) {
        printf("Error! Received: '%d' command, not 'IN' command.\r\n", setupInvitation.command);
        return;
    }

    printf("invitation received from %s:%d... sending response... \r\n", _peer_address.get_ip_address(), _peer_address.get_port());

    /* Send response packet on midi port */
    _midi_socket.sendto(_peer_address, &setupResponse, sizeof(exchange_packet));

    printf("Response sent \r\n");

    /* Set up synchronization */
    Event<void()> sync_event = _rtp_queue.event(this, &RTPMIDI::_synchronise);
    sync_event.period(50s);
    sync_event.post();
    printf("Size of EVENTS_EVENT_SIZE: %d\r\n", EVENTS_EVENT_SIZE);
    printf("Size of sync func: %d\r\n", sizeof(sync_event));
    _rtp_queue.dispatch_forever();
    _rtp_thread.start(callback(&_rtp_queue, &EventQueue::dispatch_forever));

}

void RTPMIDI::write(MIDIMessage msg)
{

    /* Check if we need to clear the buffer */
    if (_out_buffer_size + msg.length > MaxPacketSize) {
        _send_midi_buffer();
    }

    /* Replace first byte (CN) with relative timestamp */
    _out_buffer[_out_buffer_pos++] = 0x0;
    for (int i = 1; i < msg.length; ++i) {
        _out_buffer[_out_buffer_pos++] = msg.data[i];
        _out_buffer_size++;
    }

}

void RTPMIDI::_init()
{
    /* Set up empty buffer - header lives at start */
    _out_buffer_pos = _out_buffer_size = sizeof(midi_packet_header);

    midi_packet_header *header_p = (midi_packet_header *) _out_buffer;
    header_p->vpxcc = VPXCC;
    header_p->mpayload = MPAYLOAD;
    header_p->sequence_number = SEQ_NR;
    header_p->sender_ssrc = SSRC_NUMBER;

}

uint64_t RTPMIDI::_calculate_current_timestamp()
{
    /* Get count from timepoint returned by now() and divide to give 100us */
    return Kernel::Clock::now().time_since_epoch().count() / 10;
}

void RTPMIDI::_send_midi_buffer()
{

}

void RTPMIDI::_synchronise()
{
        printf("Synch starting \r\n");
        /* Receive Synchronisation CK0 */
        timestamp_packet synch;
        _midi_socket.recvfrom(&_peer_address, &synch, sizeof(timestamp_packet));

        /* Send Synchronisation CK1 */
        synch.sender_ssrc = SSRC_NUMBER;
        synch.count = SYNC_CK1;
        // network byte order
        synch.timestamp[SYNC_CK1] = htonll(_calculate_current_timestamp());
        _midi_socket.sendto(_peer_address, &synch, sizeof(exchange_packet));

        /* Receive synchronisation CK2 */
        _midi_socket.recvfrom(&_peer_address, &synch, sizeof(timestamp_packet));
}
