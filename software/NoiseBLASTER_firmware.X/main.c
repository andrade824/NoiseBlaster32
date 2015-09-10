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
#include "fat.h"

#define NUM_SECTORS 60

extern WAV_HEADER wavHeader;

enum buffer_type { FRONT, BACK };

// Flags to tell main to update data
volatile bool frontbuffer_done_sending = 0;
volatile bool backbuffer_done_sending = 0;
volatile enum buffer_type cur_buffer = FRONT;

volatile bool playing = true;

// Buffers to store audio data
int8_t frontbuffer[SECTOR_SIZE];
int8_t backbuffer[SECTOR_SIZE];

//Button Timer Flags
volatile bool vol_minus_pressed = false;
volatile bool play_pressed = false;
volatile bool vol_plus_pressed = false;
volatile bool vol_minus_held = false;
volatile bool play_held = false;
volatile bool vol_plus_held = false;

// Variables needed for FAT
#define MAX_FILES 100
struct FatPartition fat;
struct FatFile files[MAX_FILES];
uint16_t num_files = 0;
uint16_t current_song = 0;
uint32_t bytes_read = 0;

// Init functions
void InitPins(void);

//Music Controls
void play();
void pause();
void prevSong();
void nextSong();

int main(int argc, char** argv) 
{
    // Set flash wait states, turn on instruction cache, and enable prefetch
    SYSTEMConfig(SYS_FREQ, SYS_CFG_WAIT_STATES | SYS_CFG_PCACHE);
    
    // Wait to completely power up before
    int i = 0;
    for(i = 0; i < 1000000; i++);
    
    // Enable multi vectored interrupts
    INTEnableSystemMultiVectoredInt(); //Do not call after setting up interrupts
    
    // Initialize each of the subsystems
    InitPins();
    InitUART1();
    InitDAC();
    InitSD();
    InitTimer25Hz();
    InitDMA();
    
    // Start up FAT stuff and open a file
    OpenFirstFatPartition(&fat);
    num_files = GetFilesByExt(&fat, files, MAX_FILES, "WAV");
    
    if(num_files == 0)
    {
        DEBUG_LED_ON();
        while(1) { }
    }
    
    Fat_read(&(files[current_song]), (void*)frontbuffer, SECTOR_SIZE);
    readWavHeader(frontbuffer);
    
    Fat_read(&(files[current_song]), (void*)frontbuffer, SECTOR_SIZE);
    Fat_read(&(files[current_song]), (void*)backbuffer, SECTOR_SIZE);
    
    // Enable global interrupts
    asm volatile("ei");
    
    //TestWavHeader();
    
    // Set the SIRQEN and CFORCE bits to start a DMA transfer
    DCH0ECONSET = 0x90;
    
    while(1){
        // Disable interrupts to avoid read/write problems when reading variables accessed by interrupts
        // Disable interrupts to avoid crashes during I2C read when an interrupt fires during transmission
        // Enable interrupts at the end of the loop
        asm volatile("di");
        if(playing){
            if (frontbuffer_done_sending){
                frontbuffer_done_sending = false;

                bytes_read = Fat_read(&(files[current_song]), (void*)frontbuffer, SECTOR_SIZE);

                if(bytes_read < SECTOR_SIZE)
                {
                    nextSong();
                }
            }

            if (backbuffer_done_sending){
                backbuffer_done_sending = false;

                bytes_read = Fat_read(&(files[current_song]), (void*)backbuffer, SECTOR_SIZE);

                if(bytes_read < SECTOR_SIZE)
                {
                    nextSong();
                }
            }
        }
        
        if (vol_plus_pressed == true){
            DAC_VolumeUP();
            vol_plus_pressed = false;
        }
        if (vol_plus_held == true){
            nextSong();
            vol_plus_held = false;
        }
        
        if (play_pressed == true){
            play_pressed = false;
            if(playing){
                pause();
            }else{
                play();
            }
        }
        else if (play_held == true){
            play_held = false;
            if(playing){
                pause();
            }else{
                play();
            }
        }
        
        if (vol_minus_pressed == true){
            DAC_VolumeDOWN();
            vol_minus_pressed = false;
        }
        if (vol_minus_held == true){
            prevSong(); 
            vol_minus_held = false;
        }
        asm volatile("ei");
    }
    
    return (EXIT_SUCCESS);
}

void play(){
    playing = true;
    DAC_DigitalControl(false);
    DmaChnIntEnable(0);
}

void pause(){
    playing = false;
    DAC_DigitalControl(true);
    DmaChnIntEnable(0);
}

void nextSong(){
    DAC_DigitalControl(true);
    if(current_song == num_files-1){
        current_song = 0;
    }else{
        current_song = (current_song + 1) % num_files;
    }
    
    ResetFile(&(files[current_song]));
    Fat_read(&(files[current_song]), (void*)frontbuffer, SECTOR_SIZE);
    readWavHeader(frontbuffer);

    Fat_read(&(files[current_song]), (void*)frontbuffer, SECTOR_SIZE);
    Fat_read(&(files[current_song]), (void*)backbuffer, SECTOR_SIZE);
    backbuffer_done_sending = false;
    frontbuffer_done_sending = false;
    DAC_DigitalControl(false);
}

void prevSong(){
    DAC_DigitalControl(true);
    if(current_song == 0){
        current_song = num_files-1;
    }else{
        current_song = (current_song - 1) % num_files;
    }
    
    ResetFile(&(files[current_song]));
    Fat_read(&(files[current_song]), (void*)frontbuffer, SECTOR_SIZE);
    readWavHeader(frontbuffer);

    Fat_read(&(files[current_song]), (void*)frontbuffer, SECTOR_SIZE);
    Fat_read(&(files[current_song]), (void*)backbuffer, SECTOR_SIZE);
    backbuffer_done_sending = false;
    frontbuffer_done_sending = false;
    DAC_DigitalControl(false);
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

static enum {
    EXCEP_IRQ = 0,            // interrupt
    EXCEP_AdEL = 4,            // address error exception (load or ifetch)
    EXCEP_AdES,                // address error exception (store)
    EXCEP_IBE,                // bus error (ifetch)
    EXCEP_DBE,                // bus error (load/store)
    EXCEP_Sys,                // syscall
    EXCEP_Bp,                // breakpoint
    EXCEP_RI,                // reserved instruction
    EXCEP_CpU,                // coprocessor unusable
    EXCEP_Overflow,            // arithmetic overflow
    EXCEP_Trap,                // trap (possible divide by zero)
    EXCEP_IS1 = 16,            // implementation specfic 1
    EXCEP_CEU,                // CorExtend Unuseable
    EXCEP_C2E                // coprocessor 2
} _excep_code;
static unsigned int _epc_code;
static unsigned int _excep_addr;

// this function overrides the normal _weak_ generic handler
void _general_exception_handler(void)
{
    asm volatile("mfc0 %0,$13" : "=r" (_excep_code));
    asm volatile("mfc0 %0,$14" : "=r" (_excep_addr));
    _excep_code = (_excep_code & 0x0000007C) >> 2;    
    while (1) {
        DEBUG_LED_ON();
    }
}