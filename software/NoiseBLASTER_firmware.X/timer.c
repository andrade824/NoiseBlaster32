/* 
 * File:   timer.c
 * Author: Alex Lao
 *
 * Created on September 5, 2015, 4:50 PM
 */
#define _SUPPRESS_PLIB_WARNING
#include <plib.h>
#include "sysclk.h"
#include "uart.h"
#include "dac.h"
#include <stdbool.h>

extern uint32_t currentSector;

volatile uint16_t vol_minus_button;
volatile uint16_t play_button;
volatile uint16_t vol_plus_button;

volatile uint8_t freqMultipler  = 2;

/*
 * Initializes the 25Hz Timer Interrupt
 */
void InitTimer25Hz()
{
    OpenTimer1(T1_ON | T1_SOURCE_INT | T1_PS_1_256, (SYS_FREQ/256)/25);

    // Set up the timer interrupt with a priority of 2
    INTEnable(INT_T1, INT_ENABLED);
    INTSetVectorPriority(INT_TIMER_1_VECTOR, INT_PRIORITY_LEVEL_2);
    INTSetVectorSubPriority(INT_TIMER_1_VECTOR, INT_SUB_PRIORITY_LEVEL_0);   
}

/*
 * Interrupt Service Routine for the 25Hz Timer
 * 
 * Debounces the buttons and detects if a button is held or short pressed
 * Toggles the debug LED to show that the PIC has booted up
 */
void __ISR(_TIMER_1_VECTOR, ipl2) Timer1Handler(void) {
    INTClearFlag(INT_T1); // Clear the interrupt flag
    WriteTimer1(0);
    
    if(!PORTBbits.RB7) { //vol_minus_button
        vol_minus_button += 1;
        if(vol_minus_button == 15){
            // Execute prev
            UART_SendString("prev");
            currentSector -= 25000;
        }
    } else {
        if(vol_minus_button >= 2 && vol_minus_button < 15){
            // Execute vol minus
            DAC_VolumeDOWN();
            UART_SendString("-");
        }
        vol_minus_button = 0;
    }
    
    if(!PORTBbits.RB11) { //play_button
        play_button += 1;
        // Execute play
        if(play_button == 3){
            UART_SendString("play");
        }
    }else {
        play_button = 0;
    }
    
    if(!PORTBbits.RB3) { //vol_plus_button
        vol_plus_button += 1;
        if(vol_plus_button == 15){
            // Execute next 
            UART_SendString("next");
            currentSector += 25000;
        }
    } else {
        if(vol_plus_button >= 2 && vol_plus_button < 15){
            // Execute vol plus
            DAC_VolumeUP();
            UART_SendString("+");
        }
        vol_plus_button = 0;
    }
    
    mPORTAToggleBits(BIT_1);
}