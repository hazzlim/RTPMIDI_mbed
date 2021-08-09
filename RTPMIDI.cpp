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

    exchange_packet_t setupInvitation;
    _control_socket.recvfrom(&_peer_address, &setupInvitation, sizeof(exchange_packet_t));
    if (setupInvitation.command != lwip_htons(INV_COMMAND)) {
        printf("Error! Received: '%d' command, not 'IN' command.\r\n", setupInvitation.command);
        return;
    }

    printf("invitation received from %s:%d... sending response... \r\n", _peer_address.get_ip_address(), _peer_address.get_port());

    /* Send response packet on control port */
    exchange_packet_t setupResponse = {
        EXCHANGE_SIGNATURE,
        lwip_htons(ACCEPT_INV_COMMAND),
        lwip_htonl(PROTOCOL_VERSION),
        setupInvitation.initiator_token,
        lwip_htonl(SSRC_NUMBER),
        NAME
    };
    _control_socket.sendto(_peer_address, &setupResponse, sizeof(exchange_packet_t));

    printf("Response sent \r\n");

    /* Try to receive Invitation Packet on midi port */
    printf("Waiting for invitation...\r\n");

    _midi_socket.recvfrom(&_peer_address, &setupInvitation, sizeof(exchange_packet_t));
    if (setupInvitation.command != lwip_htons(INV_COMMAND)) {
        printf("Error! Received: '%d' command, not 'IN' command.\r\n", setupInvitation.command);
        return;
    }

    printf("invitation received from %s:%d... sending response... \r\n", _peer_address.get_ip_address(), _peer_address.get_port());

    /* Send response packet on midi port */
    _midi_socket.sendto(_peer_address, &setupResponse, sizeof(exchange_packet_t));

    printf("Response sent \r\n");

    /* Set up synchronization */
    Event<void()> sync_event = _rtp_queue.event(this, &RTPMIDI::_synchronise);
    sync_event.period(50s);
    sync_event.post();
    _rtp_thread.start(callback(&_rtp_queue, &EventQueue::dispatch_forever));

}

void RTPMIDI::write(MIDIMessage msg)
{
    printf("write start\r\n");

    /* Check if we need to clear the buffer */
    if (_out_buffer_size + msg.length > MAX_MIDI_COMMAND_LEN) {
        _send_midi_buffer();
    }

    /* Replace first byte (CN) with relative timestamp */
    _out_buffer[_out_buffer_pos++] = 0x0;
    for (int i = 1; i < msg.length; ++i) {
        _out_buffer[_out_buffer_pos++] = msg.data[i];
    }
    _out_buffer_size += msg.length;

    printf("write end\r\n");

}

void RTPMIDI::_init()
{
    printf("init start\r\n");

    //TODO: Support variable length MIDI Command headers

    /* Set up empty buffer - headers live at start */
    _out_buffer_size = 0;
    _out_buffer_pos = MidiHeaderLength;

    midi_packet_header_t *header_p = (midi_packet_header_t *) _out_buffer;
    header_p->vpxcc = VPXCC;
    header_p->mpayload = MPAYLOAD;
    header_p->sequence_number = SEQ_NR;
    header_p->sender_ssrc = SSRC_NUMBER;

    // B = 0, J = 0, Z = 1, P = 0, LEN = 0000
    midi_command_header_t *command_header_p = &_out_buffer[sizeof(midi_packet_header_t)];
    *command_header_p = 0;
    *command_header_p &= ~RTP_CS_FLAG_B;
    *command_header_p &= ~RTP_CS_FLAG_J;
    *command_header_p |= RTP_CS_FLAG_Z;
    *command_header_p &= ~RTP_CS_FLAG_P;

    /* Initialize sequence number */
    _sequence_number = SEQ_NR;

    printf("init end\r\n");
}

uint64_t RTPMIDI::_calculate_current_timestamp()
{
    /* Get count from timepoint returned by now() and divide to give 100us */
    return Kernel::Clock::now().time_since_epoch().count() / 10;
}

void RTPMIDI::_send_midi_buffer()
{
    printf("send start\r\n");

    /* Set timestamp */
    midi_packet_header_t *header_p = (midi_packet_header_t *) _out_buffer;
    header_p->timestamp = lwip_htonl((uint32_t)_calculate_current_timestamp());

    header_p->sender_ssrc = lwip_htonl(SSRC_NUMBER);
    header_p->sequence_number = lwip_htons(_sequence_number++);

    /* Clear and set LEN field */
    midi_command_header_t *command_header_p = &_out_buffer[sizeof(midi_packet_header_t)];
    *command_header_p &= ~RTP_CS_LEN;
    *command_header_p |= (_out_buffer_size & RTP_CS_LEN);

    /* Send the buffer */
    _midi_socket.sendto(_peer_address, _out_buffer, MidiHeaderLength + _out_buffer_size);

    /* Reset Buffer */
    _out_buffer_pos = MidiHeaderLength;
    _out_buffer_size = 0;


    printf("send end\r\n");
}

void RTPMIDI::_synchronise()
{
        printf("Synch starting \r\n");
        /* Receive Synchronisation CK0 */
        timestamp_packet_t synch;
        _midi_socket.recvfrom(&_peer_address, &synch, sizeof(timestamp_packet_t));

        /* Send Synchronisation CK1 */
        synch.sender_ssrc = lwip_htonl(SSRC_NUMBER);
        synch.count = SYNC_CK1;
        // network byte order
        synch.timestamp[SYNC_CK1] = htonll(_calculate_current_timestamp());
        _midi_socket.sendto(_peer_address, &synch, sizeof(exchange_packet_t));

        /* Receive synchronisation CK2 */
        _midi_socket.recvfrom(&_peer_address, &synch, sizeof(timestamp_packet_t));
}
