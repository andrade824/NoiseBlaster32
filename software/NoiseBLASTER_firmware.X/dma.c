#define _SUPPRESS_PLIB_WARNING
#include <xc.h>
#include <plib.h>
#include <stdint.h>
#include <sys/kmem.h>
#include <stdbool.h>
#include <math.h>
#include "sd.h"
#include "uart.h"
#include "timer.h"

// Which buffer that was just sent out
enum buffer_type { FRONT, BACK };
extern volatile enum buffer_type cur_buffer;

// Flags to tell main thread to update data
extern volatile bool frontbuffer_done_sending;
extern volatile bool backbuffer_done_sending;

// Buffers to store audio data
extern int16_t frontbuffer[SECTOR_SIZE];
extern int16_t backbuffer[SECTOR_SIZE];

/**
 * Initialize the DMA
 */
void InitDMA(void)
{   
    DmaChnIntDisable(0);    // Disable channel 0 interrupts
    DmaChnClrIntFlag(0);    // Clear interrupt flags
    //DmaChnSetIntPriority(0, INT_PRIORITY_LEVEL_7, INT_SUB_PRIORITY_LEVEL_0); 
    mDmaChnSetIntPriority(0, 7, 1);
    
    DMACONSET = 0x8000;     // Enable the DMA controller
    
    DCH0SSA = KVA_TO_PA(frontbuffer);  // Source address is front buffer for audio
    DCH0DSA = KVA_TO_PA(&SPI1BUF);   // Destination is SPI1 transmit register
    DCH0SSIZ = 512;   // Size of buffer
    DCH0DSIZ = 2;   // Size of SPI transmit register (16-bit mode)
    DCH0CSIZ = 2;   // Two bytes per SPI transfer request

    // Channel 0 Settings
    DCH0CON = 0x43;          // Channel off, pri 3, allow events while disabled
    DCH0CON = 0x53;
    DCH0ECON = 0x2600;      // Set start IRQ to 38 (SPI1 transmit)
    
    DCH0INTCLR = 0x8;       // Clear block transfer complete interrupt
    DCH0INTSET = 0x80000;   // Enable block transfer complete interrupt
    DmaChnIntEnable(0);     // Enable the interrupt in the interrupt controller
    
    DmaChnEnable(0);        // Enable the channel
}

/*
 * Finished sending one complete buffer
 * 
 * DMA channel 0 block complete interrupt service routine
 */

#pragma interrupt DmaCh0Int IPL7 vector 40
void DmaCh0Int(void)
{
    //IFS1CLR = 0x10000000;  // Clear channel 0 flag in interrupt controller
    DmaChnClrIntFlag(0);
    DCH0INTCLR = 0x8;   // Clear block transfer complete interrupt
    
    // If we just sent the front buffer, notify main thread
    if(cur_buffer == FRONT)
    {
        // Start sending backbuffer
        DCH0SSA = KVA_TO_PA(backbuffer);
        cur_buffer = BACK;
        
        frontbuffer_done_sending = true;
    }
    else
    {
        // Start sending frontbuffer
        DCH0SSA = KVA_TO_PA(frontbuffer);
        cur_buffer = FRONT;
        
        backbuffer_done_sending = true;
    }

    // Re-enable the channel, a start IRQ will trigger a transfer
    DCH0CONSET = 0x80;
}