#include "sd.h"

/**
 * Initialize SPI2 (used to interface with the SD Card)
 */
static void InitSPI2(void)
{
    SET_SS();   // De-select the SD card

    // Init the spi module for a slow (init) clock speed, 8 bit byte mode
    SPI2CONbits.ON = 0; // Disable SPI for configuration
    
    SPI2BUF;    // Clear the receive buffer
    SPI2STATbits.SPIROV = 0;    // Clear the receive overflow bit
    SPI2CON = 0x260;    // Master, CKE=0; CKP=1, sample end (dude on internet says this works)
    SPI2BRG = 128;  // Divide by 512. SCK = Fpb/(2 * (BRG + 1)), aka, 156.25KHz
    
    SPI2CONbits.ON = 1; // enable
}

/**
 * Initialize the SPI connected to the SD Card, and the SD Card itself
 */
void InitSD(void)
{
    int i = 0;
    uint8_t response = 0;
    
    // Initialize the SPI
    InitSPI2();
    
    SD_Disable();   // 1. start with the card not selected
    for ( i = 0; i < 10; i++)   // 2. send 80 clock cycles so card can init registers
        SD_Clock();
    SD_Enable();    // 3. now select the card
    
    // 4. send a reset command and look for "IDLE"
    response = SD_SendCmd(0, 0, 0x95); 
    if (response != 1) {
        SD_Disable();
        UART_SendString("Response: ");
        UART_SendInt(response);
        UART_SendString("\n\rError: Card isn't in IDLE state out of reset. Did you plug it in and apply power?\n\r");
        return;
    }
    
    // 5. Attempt to initialize the card
    response = 0xFF;    // Make sure the response isn't zero before starting loop
    while(response != 0x00)
    {
        // need to constantly send this command and wait until response is zero
        response = SD_SendCmd(1, 0, 0);
    }
    
    // 6. Set the block size to 512 bytes
    SD_SendCmd(16, SECTOR_SIZE, 0);
    
    // 7. Reconfigure the SPI to use a faster clock now that we know the SD card works
    SET_SS();   // De-select the SD card

    SPI2CONbits.ON = 0; // Disable SPI for configuration
    SPI2BRG = 0;  // SCK = Fpb/(2 * (BRG + 1))
    SPI2CONbits.ON = 1; // enable
    
    //UART_SendString("Card is initialized (theoretically, anyways ._.)\n\r");
}

/**
 * Send out a single byte and receive a byte
 * 
 * @param data The byte of data to send
 * 
 * @return The byte of data that was received
 */
uint8_t SPI_Write(uint8_t data)
{
    SPI2BUF = data;                    // write to buffer for TX
    while(!SPI2STATbits.SPIRBF);    // wait for transfer to complete
    SPI2STATbits.SPIROV = 0;        // clear any overflow.

    return SPI2BUF;                    // read the received value
}

/**
 *  Send an SPI mode command to the SD card.
 *
 *  @param cmd  The SD card command to send
 *  @param addr The 32-bit argument to pass along with the command.
 *                  Typically an address for a read/write.
 *  @param crc  The CRC value for this command, only used for the init command.
 * 
 *  @retval status read back from SD card (0xFF is a fault)
 *
 *    Expected return responses:
 *      FF - timeout 
 *      00 - command accepted
 *      01 - command received, card in idle state after RESET
 *
 *    R1 response codes (how to decode the response for most commands):
 *      bit 0 = Idle state
 *      bit 1 = Erase Reset
 *      bit 2 = Illegal command
 *      bit 3 = Communication CRC error
 *      bit 4 = Erase sequence error
 *      bit 5 = Address error
 *      bit 6 = Parameter error
 *      bit 7 = Always 0
 */
uint8_t SD_SendCmd(uint8_t cmd, uint32_t addr, uint8_t crc)
 {
    uint16_t n;
    uint8_t res;

    SD_Enable(); // enable SD card

    SPI_Write(cmd | 0x40); // send command packet (6 bytes)
    SPI_Write((addr>>24) & 0xFF); // msb of the address/argument
    SPI_Write((addr>>16) & 0xFF);
    SPI_Write((addr>>8) & 0xFF);
    SPI_Write((addr) & 0xFF); // lsb
    SPI_Write(crc); // Not used unless CRC mode is turned back on for SPI access.

    // TODO: Change this to a for loop, should be easier to read
    n = 9; // now wait for a response (allow for up to 8 bytes delay)
    do {
        res = SD_Read(); // check if ready   
        if ( res != 0xFF) 
            break;
    } while ( --n > 0);

    SD_Disable();
    
    return (res); // return the result that we got from the SD card.
 }

/**
 * Reads a single sector from the SD Card
 * 
 * @param buffer A 512 byte buffer to store the sector in
 * @param sector_num Which sector to read
 */
void SD_ReadSector(uint8_t * buffer, uint32_t sector_num)
{
    uint16_t i = 0;
    uint32_t addr = sector_num * SECTOR_SIZE;
    
    SD_Enable(); // enable SD card

    SPI_Write(17 | 0x40); // send command packet (6 bytes)
    SPI_Write((addr>>24) & 0xFF); // msb of the address/argument
    SPI_Write((addr>>16) & 0xFF);
    SPI_Write((addr>>8) & 0xFF);
    SPI_Write((addr) & 0xFF); // lsb
    SPI_Write(0); // Not used unless CRC mode is turned back on for SPI access.

    // Wait for zero confirming the command was received correctly
    for(i = 0; i < 10 && SD_Read() != 0x00; ++i);
    
    if(i >= 10)
        UART_SendString("ERROR: Data command not sent successfully\r\n");
    
    // Wait for the start of the data (aka, the 0xFE data token for CMD17)
    while(SD_Read() != 0xFE);
    
    // Actually read the data
    for(i = 0; i < SECTOR_SIZE; ++i)
        buffer[i] = SD_Read();
    
    // Skip the 2 bytes of checksum
    SD_Read();
    SD_Read();
    
    SD_Disable();
}

/**
 * Read multiple sectors from the SD Card in one transaction
 * 
 * @param buffer The buffer to store each of the sectors to read
 * @param start_sector_num The number of the starting sector to read
 * @param num_sectors The number of sectors to read
 */
void SD_ReadMultiSectors(uint8_t buffer[][SECTOR_SIZE], uint32_t start_sector_num, uint32_t num_sectors)
{
    uint16_t i, j;
    uint32_t addr = start_sector_num * SECTOR_SIZE;
    
    SD_Enable(); // enable SD card

    SPI_Write(18 | 0x40); // send command packet (6 bytes)
    SPI_Write((addr>>24) & 0xFF); // msb of the address/argument
    SPI_Write((addr>>16) & 0xFF);
    SPI_Write((addr>>8) & 0xFF);
    SPI_Write((addr) & 0xFF); // lsb
    SPI_Write(0); // Not used unless CRC mode is turned back on for SPI access.

    // Wait for zero confirming the command was received correctly
    for(i = 0; i < 10 && SD_Read() != 0x00; ++i);
    
    if(i >= 10)
        UART_SendString("ERROR: Data command not sent successfully\r\n");
    
    // Read in the sectors
    for(i = 0; i < num_sectors; ++i)
    {
        // Wait for the start of the data (aka, the 0xFE data token for CMD18)
        while(SD_Read() != 0xFE);

        // Actually read the data
        for(j = 0; j < SECTOR_SIZE; ++j)
            buffer[i][j] = SD_Read();

        // Skip the 2 bytes of checksum
        SD_Read();
        SD_Read();
    }
    
    // Send CMD12 to tell the SD card to stop transmitting
    SPI_Write(12 | 0x40); // send command packet (6 bytes)
    SPI_Write(0);
    SPI_Write(0);
    SPI_Write(0);
    SPI_Write(0);
    SPI_Write(0); // Not used unless CRC mode is turned back on for SPI access.
    
    // Ignore useless byte right after CMD12
    SD_Read();
    
    // Read response and ignore it
    for(i = 0; i < 9 && SD_Read() == 0xFF; ++i);
    
    // Wait for busy signal to de-assert
    while(SD_Read() != 0xFF);
    
    SD_Disable();
}