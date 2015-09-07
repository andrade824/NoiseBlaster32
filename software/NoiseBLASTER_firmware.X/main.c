/*  
 * File:   main.c
 * Author: Devon
 *
 * Created on August 17, 2015, 8:23 PM
 */
#define _SUPPRESS_PLIB_WARNING
 
#pragma config FPLLMUL = MUL_21, FPLLIDIV = DIV_4, FPLLODIV = DIV_2, FWDTEN = OFF
#pragma config POSCMOD = HS, FNOSC = PRIPLL, FPBDIV = DIV_1, ICESEL = ICS_PGx1

#pragma config FSOSCEN = OFF //SOSC OFF
#pragma config JTAGEN = OFF //JTAG OFF

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
#include "i2c.h"
#include "dac.h"
#include "timer.h"
#include "wav.h"

#define NUM_SECTORS 60

uint32_t currentSector = 2;

// Flags to tell main to update data
volatile bool frontbuffer_done_sending = 0;
volatile bool backbuffer_done_sending = 0;

// Buffers to store audio data
int16_t frontbuffer[SECTOR_SIZE];
int16_t backbuffer[SECTOR_SIZE];

// Init functions
void InitPins(void);
void TestSDCard(void);
void TestDMA(void);
void TestWavHeader();

int main(int argc, char** argv) 
{
    // Set flash wait states, turn on instruction cache, and enable prefetch
    SYSTEMConfig(SYS_FREQ, SYS_CFG_WAIT_STATES | SYS_CFG_PCACHE);
    
    // Wait to completely power up before
    int i = 0;
    for(i = 0; i < 1000000; i++);
    
    // Enable multi vectored interrupts
    INTEnableSystemMultiVectoredInt(); //Do not call after setting up interrupts
    // Enable global interrupts
    asm volatile("ei");
    
    // Initialize each of the subsystems
    InitPins();
    InitUART1();
    InitDAC();
    InitSD();
    InitTimer25Hz();
    InitDMA();
    
    TestWavHeader();
    
    
    SD_ReadSector(frontbuffer, 0);
    SD_ReadSector(backbuffer, 1);
    // Set the SIRQEN and CFORCE bits to start a DMA transfer
    DCH0ECONSET = 0x90;
    
    // Set the SIRQEN and CFORCE bits to start a DMA transfer
    DCH0ECONSET = 0x90;
    
    while(1){
        for(i = 0; i < 100; i++); //Avoid Race Conditions
        
        if (frontbuffer_done_sending){
            frontbuffer_done_sending = false;
            
            SD_ReadSector(frontbuffer, currentSector);
            
            //UART_SendInt(currentSector);
            //UART_SendNewLine();
            
            UART_SendByte(0xFF);
            UART_SendByte(0xAA);
            UART_SendByte(0xFF);
            
            UART_SendByte(currentSector >> 24);
            UART_SendByte(currentSector >> 16);
            UART_SendByte(currentSector >> 8);
            UART_SendByte(currentSector);
            
            currentSector += 1;
        }
        
        if (backbuffer_done_sending){
            backbuffer_done_sending = false;
            
            SD_ReadSector(backbuffer, currentSector);
            
            //UART_SendInt(currentSector);
            //UART_SendNewLine();
            
            UART_SendByte(0xFF);
            UART_SendByte(0xAA);
            UART_SendByte(0xFF);
            
            UART_SendByte(currentSector >> 24);
            UART_SendByte(currentSector >> 16);
            UART_SendByte(currentSector >> 8);
            UART_SendByte(currentSector);
            
            currentSector += 1;
        }  
    }
    return (EXIT_SUCCESS);
}

void InitPins(void)
{
    // Set all pins to be digital (no analog 4 u)
    ANSELA = 0;
    ANSELB = 0;
    
    // Debug LED Pin
    mPORTAClearBits(BIT_1);
    mPORTASetPinsDigitalOut(BIT_1);
    mPORTASetBits(BIT_1);
   
    //Volume Minus Button Pin
    mPORTBClearBits(BIT_7);
    mPORTBSetPinsDigitalIn(BIT_7);
    //Play Button Pin
    mPORTBClearBits(BIT_11);
    mPORTBSetPinsDigitalIn(BIT_11);
    //Volume Plus Button Pin
    mPORTBClearBits(BIT_3);
    mPORTBSetPinsDigitalIn(BIT_3);
    
    // SS on SPI1 (SD Card) Pin
    mPORTBClearBits(BIT_10);
    mPORTBSetPinsDigitalOut(BIT_10);
    mPORTBSetBits(BIT_10);
    
    /* All of the Peripheral Pin Select (PPS) Configuration */
    
    // I2S (SPI1) PPS
    mPORTASetPinsDigitalOut(BIT_0);     // Slave Select 1 (SS1)
    mPORTBSetPinsDigitalOut(BIT_5);     // Serial Digital Out (SDO1)
    mPORTBSetPinsDigitalOut(BIT_2);     // I2S Master Clock (REFCLKO)
    mPORTBSetPinsDigitalOut(BIT_14);    // Shift Clock 1 (SCK1)
    
    // SD Card (SPI2) PPS
    mPORTBSetPinsDigitalIn(BIT_13);     // Serial Data In 2 (SDI2)
    mPORTBSetPinsDigitalOut(BIT_10);    // Slave Select 2 (SS2)
    mPORTASetPinsDigitalOut(BIT_4);      // Serial Data Out 2 (SDO2)
    mPORTBSetPinsDigitalOut(BIT_15);    // Shift Clock 2 (SCK2)
    
    // UART PPS
    mPORTBSetBits(BIT_4); //UART1 TX Pin
    mPORTBSetPinsDigitalOut(BIT_4);     // UART 1 Transmit (U1TX)
 
    //Pin mapping Config - See Table 11-1 and 11-2 in the PIC32MX1XX/2XX Data Sheet
    PPSUnLock; // Allow PIN Mapping
    PPSOutput(1, RPA0, SS1);
    PPSOutput(2, RPB5, SDO1);
    PPSOutput(3, RPB2, REFCLKO);
    
    PPSInput(3, SDI2, RPB13);
    PPSOutput(4, RPB10, SS2);
    PPSOutput(3, RPA4, SDO2);
    
    PPSOutput(1, RPB4, U1TX);
    PPSLock; // Prevent Accidental Mapping
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

void TestWavHeader(){
    
    extern WAV_HEADER wavHeader;
    uint8_t wavSector[SECTOR_SIZE];

    SD_ReadSector(wavSector, 0);
    readWavHeader(wavSector);
    
    UART_SendNewLine();
    UART_SendString(wavHeader.riffChunk);
    UART_SendNewLine();
    UART_SendInt(mergeUnsignedInt(wavHeader.fileSize,4));
    UART_SendNewLine();
    UART_SendString(wavHeader.format);
    UART_SendNewLine();
    UART_SendString(wavHeader.formatChunk);
    UART_SendNewLine();
    UART_SendInt(mergeUnsignedInt(wavHeader.formatChunkSize,4));
    UART_SendNewLine();
    UART_SendInt(mergeUnsignedInt(wavHeader.audioFormat,2));
    UART_SendNewLine();
    UART_SendInt(mergeUnsignedInt(wavHeader.channelCount,2));
    UART_SendNewLine();
    UART_SendInt(mergeUnsignedInt(wavHeader.sampleRate,4));
    UART_SendNewLine();
    UART_SendInt(mergeUnsignedInt(wavHeader.bytesPerSecond,4));
    UART_SendNewLine();
    UART_SendInt(mergeUnsignedInt(wavHeader.blockAlign,2));
    UART_SendNewLine();
    UART_SendInt(mergeUnsignedInt(wavHeader.bitsPerSample,2));
    UART_SendNewLine();
    UART_SendString(wavHeader.dataChunk);
    UART_SendNewLine();
    UART_SendNewLine();
}