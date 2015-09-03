#define _SUPPRESS_PLIB_WARNING
#include <xc.h>
#include <plib.h>
#include <stdint.h>
#include <sys/kmem.h>
#include <stdbool.h>
#include "sd.h"
#include "uart.h"

// Which buffer that was just sent out
enum buffer_type { FRONT, BACK };
static volatile enum buffer_type cur_buffer = FRONT;

// Flags to tell main thread to update data
extern volatile bool frontbuffer_done_sending;
extern volatile bool backbuffer_done_sending;

// Buffers to store audio data
extern uint8_t frontbuffer[SECTOR_SIZE];
extern uint8_t backbuffer[SECTOR_SIZE];

// counter used to stop DMA transfers
static volatile uint8_t num_times = 0;

/**
 * Initialize the DMA
 */
void InitDMA(void)
{
    DmaChnIntDisable(0);    // Disable channel 0 interrupts
    DmaChnClrIntFlag(0);    // Clear interrupt flags
    
    DMACONSET = 0x8000;     // Enable the DMA controller
    
    // Channel 0 (frontbuffer) Settings
    DCH0CON = 0x43;          // Channel off, pri 3, allow events while disabled
    DCH0ECON = 0x2600;      // Set start IRQ to 38 (SPI1 transmit)

    DCH0SSA = KVA_TO_PA(frontbuffer);  // Source address is front buffer for audio
    DCH0DSA = KVA_TO_PA(&SPI1BUF);   // Destination is SPI1 transmit register
    DCH0SSIZ = SECTOR_SIZE;   // Size of buffer
    DCH0DSIZ = 1;   // Size of SPI transmit register (8-bit mode)
    DCH0CSIZ = 1;   // one byte per UART transfer request

    DmaChnClrIntFlag(0);
    DmaChnSetIntPriority(0, 7, 0);
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
#pragma interrupt DmaCh0Int IPL7 vector 36
void DmaCh0Int(void)
{
    IFS1CLR = 0x10000;  // Clear channel 0 flag in interrupt controller
    DCH0INTCLR = 0x8;   // Clear block transfer complete interrupt
    
    if(++num_times < 5)
    {
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
}
