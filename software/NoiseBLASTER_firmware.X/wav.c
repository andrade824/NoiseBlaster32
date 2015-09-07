/* 
 * File:   wav.c
 * Author: Alex Lao
 *
 * Created on September 6, 2015, 2:34 PM
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "wav.h"

/**
 * MExtracts a specific amount of data from the sourcePtr at a specific address 
 * 
 * @param sourcePtr A pointer to the first byte of the source
 * @param destinationPtr A pointer to the first byte of the destination
 * @param address The position of the first byte to read from the source
 * @param count The number of bytes to copy from source to destination
 */
void extractData (uint8_t * sourcePtr, uint8_t * destinationPtr, unsigned int address, unsigned int count){
    int i;
    
    for (i = 0; i<count; i++){
        destinationPtr[i] = sourcePtr [address+i];
    }
}

/**
 * Merges multiple bytes into an integer
 * 
 * @param ptr A pointer to the first byte to be read
 * @param size The number of bytes to assemble into an integer (usually 2 or 4)
 */
unsigned int mergeUnsignedInt (uint8_t * ptr, unsigned int size){
    unsigned int output = 0;
    int i;
    
    for (i = 0; i<size; i++) {
        output = output | (ptr[i] << (i * 8));
    }
    return output;
}


/**
 * Attempts to read the entire WAV header from the data provided
 * 
 * @param sourcePtr A pointer to the byte array to read from
 */
void readWavHeader (uint8_t * sourcePtr){
    extractData(sourcePtr, wavHeader.riffChunk,0,4);
    extractData(sourcePtr, wavHeader.fileSize,4,4);
    extractData(sourcePtr, wavHeader.format,8,4);
    extractData(sourcePtr, wavHeader.formatChunk,12,4);
    extractData(sourcePtr, wavHeader.formatChunkSize,16,4);
    extractData(sourcePtr, wavHeader.audioFormat,20,2);
    extractData(sourcePtr, wavHeader.channelCount,22,2);
    extractData(sourcePtr, wavHeader.sampleRate,24,4);
    extractData(sourcePtr, wavHeader.bytesPerSecond,28,4);
    extractData(sourcePtr, wavHeader.blockAlign,32,2);
    extractData(sourcePtr, wavHeader.bitsPerSample,34,2);
    extractData(sourcePtr, wavHeader.dataChunk,36,4);
    extractData(sourcePtr, wavHeader.dataChunkSize,40,4);
}