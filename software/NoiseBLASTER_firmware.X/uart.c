#define _SUPPRESS_PLIB_WARNING

#include <plib.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "sysclk.h"

/**
 * Initialize the UART
 */
void InitUART1(void)
{
    UARTConfigure(UART1, UART_ENABLE_PINS_TX_RX_ONLY);
    UARTSetFifoMode(UART1, UART_INTERRUPT_ON_TX_NOT_FULL | UART_INTERRUPT_ON_RX_NOT_EMPTY);
    UARTSetLineControl(UART1, UART_DATA_SIZE_8_BITS | UART_PARITY_NONE | UART_STOP_BITS_1);
    UARTSetDataRate(UART1, (SYS_FREQ/(1 << OSCCONbits.PBDIV)), 115200);
    UARTEnable(UART1, UART_ENABLE_FLAGS(UART_PERIPHERAL | UART_RX | UART_TX));
}

/**
 * Send out a single byte of data over the UART
 * 
 * @param data The byte of data to send
 */
void UART_SendByte(uint8_t data)
{
    while (!UARTTransmitterIsReady(UART1));
    UARTSendDataByte(UART1, data);
}

/**
 * Print out a string over the UART
 * 
 * @param buffer The string to print out
 */
void UART_SendString(char *buffer) {
    while (*buffer != (char) 0) {
        while (!UARTTransmitterIsReady(UART1));
        UARTSendDataByte(UART1, *buffer++);
    }
}

/**
 * Print out an integer over the UART
 * 
 * @param value The integer value to print as a string
 */
void UART_SendInt(unsigned int value) {
    char numstr[10]; // For number and end of string
    sprintf(numstr, "%d", value);
    UART_SendString(numstr);
}
