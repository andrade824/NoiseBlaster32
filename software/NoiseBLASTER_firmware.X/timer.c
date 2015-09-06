/* 
 * File:   timer.c
 * Author: Alex Lao
 *
 * Created on September 5, 2015, 4:50 PM
 */

#include <plib.h>
#include "sysclk.h"

void InitTimer25Hz()
{
    OpenTimer1(T1_ON | T1_SOURCE_INT | T1_PS_1_1, SYS_FREQ/250);

    // Set up the timer interrupt with a priority of 2
    INTEnable(INT_T1, INT_ENABLED);
    INTSetVectorPriority(INT_TIMER_1_VECTOR, INT_PRIORITY_LEVEL_2);
    INTSetVectorSubPriority(INT_TIMER_1_VECTOR, INT_SUB_PRIORITY_LEVEL_0);   
}

void __ISR(_TIMER_1_VECTOR, ipl2) Timer1Handler(void) {
    INTClearFlag(INT_T1); // Clear the interrupt flag
    WriteTimer1(0);
    
    mPORTAToggleBits(BIT_1);
}