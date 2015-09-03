/* 
 * File:   main.c
 * Author: Devon
 *
 * Created on August 17, 2015, 8:23 PM
 */
#define _SUPPRESS_PLIB_WARNING
 
#pragma config FPLLMUL = MUL_20, FPLLIDIV = DIV_2, FPLLODIV = DIV_2, FWDTEN = OFF
#pragma config POSCMOD = HS, FNOSC = PRIPLL, FPBDIV = DIV_1

#include <plib.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "debug.h"
#include "sysclk.h"
#include "sd.h"
#include "uart.h"
#include "dma.h"

#define NUM_SECTORS 8

// Flags to tell main to update data
volatile bool frontbuffer_done_sending = 0;
volatile bool backbuffer_done_sending = 0;

// Buffers to store audio data
uint8_t frontbuffer[SECTOR_SIZE];
uint8_t backbuffer[SECTOR_SIZE];

// Init functions
void InitGPIO(void);
void TestSDCard(void);
void TestDMA(void);

int main(int argc, char** argv) 
{
    // Set flash wait states, turn on instruction cache, and enable prefetch
    SYSTEMConfig(SYS_FREQ, SYS_CFG_WAIT_STATES | SYS_CFG_PCACHE);
    
    // Initialize each of the subsystems
    InitGPIO();
    InitUART1();
    InitSD();
    InitDMA();
    
    // Enable multi vectored interrupts
    INTCONSET = 0x1000;
    
    // Enable global interrupts
    asm volatile("ei");
    
    TestDMA();
    
    return (EXIT_SUCCESS);
}

void InitGPIO(void)
{
    // Debug LED Pin
    mPORTAClearBits(BIT_1);
    mPORTASetPinsDigitalOut(BIT_1);
    mPORTASetBits(BIT_1);
   
    // SS on SPI1 (SD Card) Pin
    mPORTBClearBits(BIT_10);
    mPORTBSetPinsDigitalOut(BIT_10);
    mPORTBSetBits(BIT_10);
}

void TestSDCard(void)
{
    int i, j;
    uint8_t buffer[NUM_SECTORS][SECTOR_SIZE];
    
    UART_SendString("Starting sector read...\r\n");
    SD_ReadMultiSectors(buffer, 0, NUM_SECTORS);
    
    UART_SendString("Dumping sectors...\r\n");
    
    for(i = 0; i < NUM_SECTORS; ++i)
    {
        UART_SendString("AABB");
        for(j = 0; j < SECTOR_SIZE; ++j)
            UART_SendByte(buffer[i][j]);
    }
    
    DEBUG_LED_ON();
    
    UART_SendString("End Test\r\n");
}

void TestDMA(void)
{
    SD_ReadSector(frontbuffer, 0);
    SD_ReadSector(backbuffer, 1);
    
    // Set the SIRQEN and CFORCE bits to start a DMA transfer
    DCH0ECONSET = 0x90;
    
    while(1)
    {
        // Frontbuffer just finished sending, load it with new data
        if(frontbuffer_done_sending)
        {
            SD_ReadSector(frontbuffer, 0);
            frontbuffer_done_sending = false;
        }
        
        // Backbuffer just finished sending, load it with new data
        if(backbuffer_done_sending)
        {
            SD_ReadSector(backbuffer, 1);
            backbuffer_done_sending = false;
        }
    }
}