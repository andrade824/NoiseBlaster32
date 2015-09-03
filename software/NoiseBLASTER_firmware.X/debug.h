/* 
 * File:   debug.h
 * Author: Devon
 *
 * Created on August 27, 2015, 5:45 PM
 */

#ifndef DEBUG_H
#define	DEBUG_H

#define DEBUG_ENABLED

// Debug macros
#ifdef DEBUG_ENABLED
    #define DEBUG_LED_ON() (mPORTAClearBits(BIT_1))
    #define DEBUG_LED_OFF() (mPORTASetBits(BIT_1))
#else
    #define DEBUG_LED0_ON()
    #define DEBUG_LED1_ON()
    #define DEBUG_LED2_ON()
    #define DEBUG_LED0_OFF()
    #define DEBUG_LED1_OFF()
    #define DEBUG_LED2_OFF()
#endif

#endif	/* DEBUG_H */

