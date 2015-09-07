/* 
 * File:   wav.h
 * Author: Alex Lao
 *
 * Created on September 6, 2015, 2:33 PM
 */

#ifndef WAV_H
#define	WAV_H

typedef struct {
    uint8_t riffChunk[5]; //Expected RIFF
    uint8_t fileSize[4]; // 32 bit unsigned int
    uint8_t format[5]; //Expected WAVE
    uint8_t formatChunk[4]; //Expected fmt
    uint8_t formatChunkSize[4]; // 32 bit unsigned int
    uint8_t audioFormat[2]; // 16 bit unsigned int
    uint8_t channelCount[2]; // 16 bit unsigned int
    uint8_t sampleRate[4]; // 32 bit unsigned int
    uint8_t bytesPerSecond[4]; // 32 bit unsigned int
    uint8_t blockAlign[2]; // 16 bit unsigned int
    uint8_t bitsPerSample[2]; // 16 bit unsigned int
    uint8_t dataChunk[5]; //Expected DATA
    uint8_t dataChunkSize[4]; // 32 bit unsigned int
} WAV_HEADER;

WAV_HEADER wavHeader;

void extractData (uint8_t * sourcePtr, uint8_t * destinationPtr, unsigned int address, unsigned int count);
unsigned int mergeUnsignedInt (uint8_t * ptr, unsigned int size);
void readWavHeader (uint8_t * sourcePtr);

#endif	/* WAV_H */

