/* mbed Microcontroller Library
 * Copyright (c) 2021 ARM Limited
 * SPDX-License-Identifier: Apache-2.0
 */

#include "mbed.h"
#include "fxos8700cq.h"

#define I2C0_SDA PTD9
#define I2C0_SCL PTD8

FXOS8700CQ acc(I2C0_SDA, I2C0_SCL);

int modulate(Data &values)
{
    return (0x3FFF * (values.ax + 1)) / 2;
}

int main()
{
    acc.init();

    while (1) {

        // poll the sensor and get the values, storing in a struct
        Data values = acc.get_values();

        // generate the bend value from the sensor values
        int bend = modulate(values);

        // print the bend value
        printf("Pitch bend value: %d\n", bend);

        ThisThread::sleep_for(500ms);
    }
}
