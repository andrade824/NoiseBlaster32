/* 
 * File:   i2c.h
 * Author: Alex Lao
 *
 * Created on September 4, 2015, 6:42 PM
 */
#ifndef I2C_H
#define	I2C_H

#define I2C_SCL_Pin BIT_8
#define I2C_SDA_Pin BIT_9

#define I2C_Timeout 10000
#define I2C_Read BIT_0
#define I2C_Clock 100000

// Function Prototypes
void InitI2C();
void I2C_StartTransfer(BOOL restart);
BOOL I2C_TransmitOneByte(unsigned char data);
void I2C_Nack();
void I2C_Ack();
unsigned char I2C_ReceiveOneByte();
void I2C_StopTransfer();

#endif	/* I2C_H */