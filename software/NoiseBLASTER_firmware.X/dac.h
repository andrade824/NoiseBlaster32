/* 
 * File:   dac.h
 * Author: Alex Lao
 *
 * Created on September 4, 2015, 6:43 PM
 */

#ifndef DAC_H
#define	DAC_H

#include <stdbool.h>

//TLV320DAC23 With CSn pulled low in I2C Mode
#define DAC_Address 0x34 //0x1A << 1

//Register Map
#define Left_LI_Control 0x00
#define Right_LI_Control 0x01
#define Left_H_Vol_Control 0x02
#define Right_H_Vol_Control 0x03
#define Analog_Path_Control 0x04
#define Digital_Path_Control 0x05
#define Power_Down_Control 0x06
#define Digital_Interface_Format 0x07
#define Sample_Rate_Control 0x08
#define Digital_Interface_Activation 0x09
#define Reset_Register 0x0A

// Function Prototypes
void InitDAC();
void InitI2S();
void DAC_Write(unsigned int registerAddress, unsigned int data);
void DAC_LineInMuteControl(bool mute);
void DAC_VolumeControl(unsigned char volume);
void DAC_AnalogControl(bool DAC_select, bool bypass);
void DAC_DigitalControl(bool DAC_mute);
void DAC_PowerDownControl(bool DAC_power, bool DAC_clk, bool DAC_osc, bool DAC_out, bool DAC_dac, bool DAC_line_in);
void DAC_DigitalAudioInterface(bool master, bool lr_swap, bool lrp);
void DAC_SampleRateControl(bool clk_out_div, bool clk_in_div);
void DAC_Digital_Interface_Activation(bool on);
void DAC_Reset();

// Volume Control Helper Functions
void DAC_VolumeUP();
void DAC_VolumeDOWN();

#endif	/* DAC_H */

