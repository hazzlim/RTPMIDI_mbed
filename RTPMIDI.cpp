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

RTPMIDI::RTPMIDI() : _net(NetworkInterface::get_default_instance())
{
}

RTPMIDI::RTPMIDI(NetworkInterface *net) : _net(net)
{
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
    SocketAddress initiatorAddress;
    _control_socket.recvfrom(&initiatorAddress, &setupInvitation, sizeof(exchange_packet));
    if (setupInvitation.command != lwip_htons(INV_COMMAND)) {
        printf("Error! Received: '%d' command, not 'IN' command.\r\n", setupInvitation.command);
        return;
    }

    printf("invitation received from %s:%d... sending response... \r\n", initiatorAddress.get_ip_address(), initiatorAddress.get_port());

    /* Send response packet on control port */
    exchange_packet setupResponse = {
        EXCHANGE_SIGNATURE,
        lwip_htons(ACCEPT_INV_COMMAND),
        lwip_htonl(PROTOCOL_VERSION),
        setupInvitation.initiator_token,
        SSRC_NUMBER,
        NAME
    };
    _control_socket.sendto(initiatorAddress, &setupResponse, sizeof(exchange_packet));

    printf("Response sent \r\n");

    /* Try to receive Invitation Packet on midi port */
    printf("Waiting for invitation...\r\n");

    _midi_socket.recvfrom(&initiatorAddress, &setupInvitation, sizeof(exchange_packet));
    if (setupInvitation.command != lwip_htons(INV_COMMAND)) {
        printf("Error! Received: '%d' command, not 'IN' command.\r\n", setupInvitation.command);
        return;
    }

    printf("invitation received from %s:%d... sending response... \r\n", initiatorAddress.get_ip_address(), initiatorAddress.get_port());

    /* Send response packet on midi port */
    _midi_socket.sendto(initiatorAddress, &setupResponse, sizeof(exchange_packet));

    printf("Response sent \r\n");

    /* Receive Synchronisation CK0 */
    timestamp_packet synch;
    _midi_socket.recvfrom(&initiatorAddress, &synch, sizeof(timestamp_packet));

    /* Send Synchronisation CK1 */
    synch.sender_ssrc = SSRC_NUMBER;
    synch.count = SYNC_CK1;
    // network byte order
    synch.timestamp[SYNC_CK1] = htonll(_calculate_current_timestamp());

    _midi_socket.sendto(initiatorAddress, &synch, sizeof(exchange_packet));
}

void RTPMIDI::write(MIDIMessage msg)
{

    /* Check if we need to clear the buffer */
    if (_out_midi_buffer.size() + msg.length > MAX_BUFFER_SIZE) {
        _send_midi_buffer();
    }

    /* First byte (CN) is ignored */
    for (int i = 1; i < msg.length; ++i) {
        _out_midi_buffer.push_back(msg.data[i]);
    }

}

uint64_t RTPMIDI::_calculate_current_timestamp()
{
    /* Get count from timepoint returned by now() and divide to give 100us */
    return Kernel::Clock::now().time_since_epoch().count() / 10;
}

