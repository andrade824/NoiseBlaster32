/* 
 * File:   uart.h
 * Author: Devon
 *
 * Created on August 27, 2015, 5:24 PM
 */

#ifndef UART_H
#define	UART_H

#include <stdint.h>

// Initialize the UART
void InitUART1(void);

// UART Helper Functions
void UART_SendByte(uint8_t data);
void UART_SendString(char *buffer);
void UART_SendInt(unsigned int value);
void UART_SendNewLine();

#endif	/* UART_H */

