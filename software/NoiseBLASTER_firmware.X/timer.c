/* 
 * File:   timer.c
 * Author: Alex Lao
 *
 * Created on September 5, 2015, 4:50 PM
 */
#define _SUPPRESS_PLIB_WARNING
#include <xc.h>
#include <plib.h>
#include "sysclk.h"
#include <stdbool.h>
#include <stdint.h>

volatile uint8_t vol_minus_button;
volatile uint8_t play_button;
volatile uint8_t vol_plus_button;

//Button Timer Flags
extern volatile bool vol_minus_pressed;
extern volatile bool play_pressed;
extern volatile bool vol_plus_pressed;
extern volatile bool vol_minus_held;
extern volatile bool play_held;
extern volatile bool vol_plus_held;

extern volatile bool playing;

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
 * Toggles the debug LED to show that the PIC is running and not in the exception handler
 */
void __ISR(_TIMER_1_VECTOR, ipl2) Timer1Handler(void) {
    INTClearFlag(INT_T1); // Clear the interrupt flag
    WriteTimer1(0);
    
    if(!PORTBbits.RB7) { //vol_minus_button
        vol_minus_button += 1;
        if(vol_minus_button == 15){
            // Execute prev
            vol_minus_held = true;
        }
    } else {
        if(vol_minus_button >= 3 && vol_minus_button < 15){
            // Execute vol minus
            vol_minus_pressed = true;
        }
        vol_minus_button = 0;
    }
    
    if(!PORTBbits.RB11) { //play_button
        play_button += 1; 
        if(play_button == 15){
            // Execute play held
            play_held = true;
        }
    }else {
        if(play_button >= 3 && play_button < 15){
            // Execute play press
            play_pressed = true;
        }
        play_button = 0;
    }
    
    if(!PORTBbits.RB3) { //vol_plus_button
        vol_plus_button += 1;
        if(vol_plus_button == 15){
            // Execute next 
            vol_plus_held = true;
        }
    } else {
        if(vol_plus_button >= 3 && vol_plus_button < 15){
            // Execute vol plus
            vol_plus_pressed = true;
        }
        vol_plus_button = 0;
    }
    if(playing){
        mPORTAToggleBits(BIT_1);
    } else{
        mPORTASetBits(BIT_1);
    }
}