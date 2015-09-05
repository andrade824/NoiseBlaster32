/* 
 * File:   i2c.c
 * Author: Alex Lao
 *
 * Created on September 4, 2015, 6:42 PM
 */
#define _SUPPRESS_PLIB_WARNING
#include <plib.h>
#include "i2c.h"
#include "sysclk.h"

/**
 * Initialize the I2C
 */
void InitI2C() {
    //I2C Pin Config
    mPORTBSetPinsDigitalOut(I2C_SCL_Pin | I2C_SDA_Pin);

    I2CEnable(I2C1, FALSE);

    //Soft reset I2C Bus by pulsing the clock line 10 times
    mPORTBSetBits(I2C_SCL_Pin | I2C_SDA_Pin);
    unsigned int i;
    unsigned int wait;
    for (i = 0; i < 20; i++) {
        for (wait = 0; wait < 20; wait++);
        mPORTBToggleBits(I2C_SCL_Pin);
    }
    mPORTBSetBits(I2C_SCL_Pin | I2C_SDA_Pin);

    // Configure Various I2C Options
    //!!!!! - Slew rate control off(High speed mode enabled), If enabled, RA0 and RA1 fail to work, see silicon errata (Microchip Hardware Bugs)
    I2CConfigure(I2C1, I2C_ENABLE_SLAVE_CLOCK_STRETCHING | I2C_ENABLE_HIGH_SPEED);
    // Set the I2C baud rate
    int I2C_actualClock = I2CSetFrequency(I2C1, SYS_FREQ, I2C_Clock);
    if (abs(I2C_actualClock - I2C_Clock) > I2C_Clock / 10) {
        //serialPrint("I2C Clock frequency (%d) error exceeds 10% = ");
        //serialPrintInt(I2C_actualClock);
        //serialPrint("Hz \r\n");

        while (1);
    } else {
        //serialPrint("I2C Clock frequency = ");
        //serialPrintInt(I2C_actualClock);
        //serialPrint("Hz \r\n");
    }
    // Enable the I2C bus
    I2CEnable(I2C1, TRUE);

    while (!I2CTransmitterIsReady(I2C1));

    // configure the interrupt priority for the I2C peripheral
    //INTSetVectorPriority(INT_I2C_1_VECTOR,INT_PRIORITY_LEVEL_3);
    //INTClearFlag(INT_I2C1);
    //INTEnable(INT_I2C1,INT_ENABLED);
}

/**
 * Invokes an I2C start condition
 * 
 * @param restart invoke a restart condition if true
 */
void I2C_StartTransfer(BOOL restart) {
    // Send the Start (or Restart) signal
    if (restart) {
        I2CRepeatStart(I2C1);
    } else {
        // Wait for the bus to be idle, then start the transfer
        while (!I2CBusIsIdle(I2C1));

        if (I2CStart(I2C1) != I2C_SUCCESS) {
            //serialPrint("Error: Bus collision during transfer Start\r\n");
            while (1);
        }
    }
    // Wait for the signal to complete
    while (!(I2CGetStatus(I2C1) & I2C_START));
}

/**
 * Send out a single byte of data over the I2C
 * 
 * @param data The byte of data to send
 */
BOOL I2C_TransmitOneByte(unsigned char data) {
    // Wait for the transmitter to be ready
    while (!I2CTransmitterIsReady(I2C1));

    // Transmit the byte
    if (I2CSendByte(I2C1, data) == I2C_MASTER_BUS_COLLISION) {
        //serialPrint("Error: I2C Master Bus Collision\r\n");
        while (1);
    }

    // Wait for the transmission to finish
    while (!I2CTransmissionHasCompleted(I2C1));

    unsigned int i;
    for (i = 0; i < I2C_Timeout; i++) {
        if (I2CByteWasAcknowledged(I2C1)) {
            break;
        } else if (i == I2C_Timeout - 1) {
            //serialPrint("Error: I2C Slave Did Not Acknowledge\r\n");
            return FALSE;
        }
    }
    return TRUE;
}

/**
 * Invokes an I2C no acknowledge
 * 
 */
void I2C_Nack() {
    I2CAcknowledgeByte(I2C1, FALSE);
    while (!I2CAcknowledgeHasCompleted(I2C1));
}

/**
 * Invokes an I2C acknowledge
 * 
 */
void I2C_Ack() {
    I2CAcknowledgeByte(I2C1, TRUE);
    while (!I2CAcknowledgeHasCompleted(I2C1));
}

//TLV320DAC23 does not support reads
/**
 * Reads out a single byte of data over the I2C
 * 
 * @return The received byte of data
 */
unsigned char I2C_ReceiveOneByte() {
    I2CReceiverEnable(I2C1, TRUE);
    while (!I2CReceivedDataIsAvailable(I2C1));
    I2CReceiverEnable(I2C1, FALSE);

    return I2CGetByte(I2C1);
}

/**
 * Invokes an I2C stop condition
 * 
 */
void I2C_StopTransfer() {
    // Send the Stop signal
    I2CStop(I2C1);

    // Wait for the signal to complete
    while (!(I2CGetStatus(I2C1) & I2C_STOP));
}