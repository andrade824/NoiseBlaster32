/* 
 * File:   sd.h
 * Author: Devon
 *
 * Created on August 27, 2015, 5:17 PM
 */

#ifndef SD_H
#define	SD_H

#define _SUPPRESS_PLIB_WARNING
#include <xc.h>
#include <plib.h>
#include <stdint.h>

// Clear/Set the SS 
#define CLEAR_SS() (mPORTBClearBits(BIT_10))
#define SET_SS() (mPORTBSetBits(BIT_10))

// SD Card helper macros
#define SD_Read()   (SPI_Write(0xFF))
#define SD_Clock()   (SPI_Write(0xFF))
#define SD_Disable() SET_SS(); SD_Clock()
#define SD_Enable()  (CLEAR_SS())

// Gets the sector number that this address lies in
#define SECTOR_NUM(addr) (addr >> 9)

// Size of a sector in bytes
#define SECTOR_SIZE 512

// The maximum number of buffers the SD_ReadData function will hold
// This reduces the maximum number of bytes that can be read from that function
// Max data that can be read: (MAX_SD_BUFFERS - 1) * SECTOR_SIZE
#define MAX_SD_BUFFERS 2

// Initialization functions
void InitSD(void);

// SPI/SD Helper Functions
uint8_t SPI_Write(uint8_t data);
uint8_t SD_SendCmd(uint8_t cmd, uint32_t addr, uint8_t crc);
void SD_ReadData(void * buffer, uint32_t start_byte, size_t size);
void SD_ReadSector(uint8_t * buffer, uint32_t sector_num);
void SD_ReadMultiSectors(uint8_t buffer[][SECTOR_SIZE], uint32_t start_sector_num, uint32_t num_sectors);

#endif	/* SD_H */

