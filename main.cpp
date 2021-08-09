/* mbed Microcontroller Library
 * Copyright (c) 2021 ARM Limited
 * SPDX-License-Identifier: Apache-2.0
 */

#include "RTPMIDI.h"
#include "mbed.h"
#include "fxos8700cq.h"

// TODO: Remove these includes:
#include "EthernetInterface.h"
#include "LWIPStack.h"

#define I2C0_SDA PTD9
#define I2C0_SCL PTD8

/* Accelerometer Interface */
FXOS8700CQ acc(I2C0_SDA, I2C0_SCL);

int modulate(Data &values)
{
    return (0x3FFF * (values.ax + 1)) / 2;
}

int main()
{
    RTPMIDI rtpMidi;
    rtpMidi.participate();

    ThisThread::sleep_for(250ms);
    rtpMidi.write(MIDIMessage::NoteOn(48));
    rtpMidi.write(MIDIMessage::NoteOff(48));
    rtpMidi.write(MIDIMessage::NoteOn(48));
    rtpMidi.write(MIDIMessage::NoteOff(48));
    while(1) {
        ThisThread::sleep_for(500ms);
    }
}
