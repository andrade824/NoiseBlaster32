/* 
 * File:   dac.c
 * Author: Alex Lao
 *
 * Created on September 4, 2015, 6:42 PM
 */
#define _SUPPRESS_PLIB_WARNING
#include <plib.h>
#include "dac.h"
#include "i2c.h"
#include <stdbool.h>

/**
 * Initialize the DAC
 */
void InitDAC()
{
    InitI2C();
    DAC_Reset();
    DAC_LineInMuteControl(1); //Line in muted
    DAC_VolumeControl(33); //Low Volume
    DAC_AnalogControl(1, 0); //Bypass enabled? Not sure if that means switch open or closed
    DAC_DigitalControl(0); //Digital mute off
    DAC_PowerDownControl(0, 0, 1, 0, 0, 1);// Power on, clock on, oscillator off, outputs on, dac on, line in off
    DAC_DigitalAudioInterface(0, 0, 0); //Slave, lr_swap off, lrp... check lrp, function default 16 bit and i2s
    DAC_SampleRateControl(0, 0); //No Dividers
    DAC_Digital_Interface_Activation(1);    // Activate the digital interface
    
    //DAC should be fully configured now
    
    InitI2S();  // Setup the SPI module to start sending audio data
}

/**
 * Initialize the I2S Module (SPI1) to 16-bit stereo mode
 */
void InitI2S()
{
    // Set up the REFCLKO
    REFOCONbits.ROSEL = 2;  // Based off of primary oscillator
    REFOTRIMbits.ROTRIM = 256;  // This will make a divisor of 1, aka, no divisor
    REFOCONbits.DIVSWEN = 1;    // Switch to the new trim value
    REFOCONbits.OE = 1;
    REFOCONbits.ON = 1;
    
    // Reset everything
    SPI1CONbits.ON = 0;
    SPI1CON2 = 0;
    SPI1BRG = 0;
    
    // Setup SPI1
    int rData = SPI1BUF;        // Clear out the receive buffer
    SPI1CONbits.ENHBUF = 1;     // We want a FIFO buffer
    SPI1CONbits.MCLKSEL = 1;    // Use REFCLK for BCLK (SCK) generation
    SPI1STATbits.SPIROV = 0;    // Clear overflow bit
    SPI1CON2bits.AUDMOD = 0;    // I2S mode
    SPI1CON2bits.AUDEN = 1;     // Enable the Audio mode
    SPI1CON2bits.AUDMONO = 1;
    SPI1BRG = 5;                // 1.4112MHz BCLK (44.1KHz * 16bits * 2)
    SPI1CONbits.STXISEL = 3;    // Trigger an interrupt when FIFO isn't full
    SPI1CONbits.MSTEN = 1;      // Master Mode
    SPI1CONbits.CKP = 1;        // Need this for I2S mode
    SPI1CONbits.MODE16 = 0;     // Need this for I2S mode
    SPI1CONbits.MODE32 = 0;     // Need this for I2S mode
    SPI1CONbits.ON = 1;         // Start transmitting
}

/**
 * Writes to a register on the DAC over I2C
 * 
 * @param registerAddress The 7 bit address you want to write to
 * @param data The 9 bit data you want to write to that register
 */
void DAC_Write(unsigned int registerAddress, unsigned int data)
{
    I2C_StartTransfer(FALSE);
    I2C_TransmitOneByte(DAC_Address); //DAC Write
    I2C_TransmitOneByte((registerAddress << 1) | ((data >> 8) & 0x1)); //registerAddress is 7 bits, data is 9 bits
    I2C_TransmitOneByte(data & 0xFF);
    I2C_StopTransfer();
}

/**
 * Writes to the line in mute control register
 * 
 * @param mute Set true to mute
 */
void DAC_LineInMuteControl(bool mute)
{
    DAC_Write(Left_LI_Control, (1<<8) | (mute << 7));
}
 
/**
 * Writes to the volume control register
 * 
 * @param volume The volume to set the DAC headphone amplifier to, valid values between 0 and 79
 */
void DAC_VolumeControl(unsigned char volume)
{
    //volume input should be between 0 and 79
    if(volume > 79) {
        volume = 79;
    }

    DAC_Write(Right_H_Vol_Control, ((1<<8) | (1<<7) | (volume + 48))); //written value below 48 = mute
}

/**
 * Writes to the line in analog control register
 * 
 * @param DAC_select Set true to turn the DAC on
 * @param bypass Set to true to bypass the DAC
 */
void DAC_AnalogControl(bool DAC_select, bool bypass)
{
    DAC_Write(Analog_Path_Control, (DAC_select << 4) | (bypass << 3) );
}

/**
 * Writes to the digital control register
 * 
 * @param DAC_mute Set true to digitally mute the DAC
 */
//HARDCODED 44.1 kHz
void DAC_DigitalControl(bool DAC_mute)
{
    DAC_Write(Digital_Path_Control, (DAC_mute << 3) | (1 << 2) );
}

/**
 * Writes to the power down control register
 * 
 * @param DAC_power Set to true to turn on the DAC chip
 * @param DAC_clk Set to true to turn on the clock
 * @param DAC_osc Set to true to turn on the oscillator
 * @param DAC_out Set to true to turn on the audio outputs
 * @param DAC_dac Set to true to turn on the DAC
 * @param DAC_line_in Set to true to turn on the line in
 */
void DAC_PowerDownControl(bool DAC_power, bool DAC_clk, bool DAC_osc, bool DAC_out, bool DAC_dac, bool DAC_line_in)
{
    DAC_Write(Power_Down_Control, DAC_power << 7 | DAC_clk << 6 | DAC_osc << 5 | DAC_out << 4 | DAC_dac << 3 | DAC_line_in );
}

/**
 * Writes to the digital audio interface register
 * 
 * @param master Set to true to set the DAC into master mode
 * @param lr_swap Set to true to swap the left and right channel
 * @param lrp Set to true to change the left right order
 */
//HARDCODED I2S, 16 bit
void DAC_DigitalAudioInterface(bool master, bool lr_swap, bool lrp)
{
    DAC_Write(Digital_Interface_Format, master << 6 | lr_swap << 5 | lrp << 4 | 1 << 1);
}
 
/**
 * Writes to the sample rate control register
 * 
 * @param clock_out_div Set to true to turn on the clock output divider
 * @param clock_in_div Set to true to turn on the clock input divider
 */
//HARDCODED normal mode, 384 fs, 16.9344 MHz, 44.1 kHz, clk_in_div should be 0
void DAC_SampleRateControl(bool clk_out_div, bool clk_in_div)
{
    DAC_Write(Sample_Rate_Control, clk_out_div << 7 | clk_in_div << 6 | 1 << 5 | 1 << 1);
}

/**
 * Writes to the digital interface activation register
 * 
 * @param on Set to true to turn on the digital audio interface
 */
void DAC_Digital_Interface_Activation(bool on)
{
    DAC_Write(Digital_Interface_Activation, on);
}

/**
 * Writes to the reset register to reset the DAC
 */
void DAC_Reset()
{
    DAC_Write(Reset_Register, 0xff);
}