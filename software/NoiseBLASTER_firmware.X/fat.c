#include <stdint.h>
#include <stdbool.h>
#include "fat.h"
#include "sd.h"

// Function prototypes
static enum FatFileType GetFileType(unsigned char first);

#define HAS_MBR
bool OpenFirstFatPartition(struct FatPartition * fat)
{
    bool found_fat_partition = false;   // Whether we found a FAT partition or not
    
    // If the sd card contains an MBR, define HAS_MBR
#ifdef HAS_MBR
    struct PartitionTable tables[MAX_MBR_PARTITIONS];   // The four partitions in the Master Boot Record
    
    int i = 0;
    // Read the partition tables from the MBR
    SD_ReadData(tables, 0x1BE, sizeof(struct PartitionTable) * MAX_MBR_PARTITIONS);

    for(i = 0; i < MAX_MBR_PARTITIONS && !found_fat_partition; ++i) {
        // If it's one of the FAT16 partition types, start filling it with data
        if (tables[i].partition_type == 4 || tables[i].partition_type == 6 || tables[i].partition_type == 14) {
            found_fat_partition = true;
            fat->start_sector = tables[i].start_sector;
            fat->partition_type = tables[i].partition_type;

            // Read boot sector then calculate helper values
            SD_ReadData(&(fat->boot), SECTOR_SIZE * fat->start_sector, sizeof(struct Fat16BootSector));
            fat->cluster_size = fat->boot.num_sectors_per_cluster * fat->boot.sector_size;
            fat->fat_start = fat->boot.sector_size * (fat->boot.reserved_sectors + fat->start_sector);
            fat->root_start = fat->fat_start + (fat->boot.sector_size * (fat->boot.fat_num_sectors * fat->boot.num_fats));
            fat->data_start = fat->root_start + (uint32_t)(fat->boot.num_root_entries * sizeof(struct Fat16Entry));
        }
    }
#else
    // If the SD Card starts off with a boot sector instead of MBR, assume it's FAT16
    found_fat_partition = true;
    fat->start_sector = 0;
    fat->partition_type = 6;    // Assuming Fat16

    // Read boot sector then calculate helper values
    SD_ReadData(&(fat->boot), 0, sizeof(struct Fat16BootSector));
    fat->cluster_size = fat->boot.num_sectors_per_cluster * fat->boot.sector_size;
    fat->fat_start = fat->boot.sector_size * (fat->boot.reserved_sectors + fat->start_sector);
    fat->root_start = fat->fat_start + (fat->boot.sector_size * (fat->boot.fat_num_sectors * fat->boot.num_fats));
    fat->data_start = fat->root_start + (uint32_t)(fat->boot.num_root_entries * sizeof(struct Fat16Entry));
#endif
    
    return found_fat_partition;
}

uint16_t GetFilesByExt(struct FatPartition * fat, struct FatFile * files, uint16_t num_files, char * ext)
{
    struct Fat16Entry entry;
    unsigned int i = 0;
    uint16_t num_found = 0;

    // Loop through every root file entry looking for matching extension
    for(i = 0; i < fat->boot.num_root_entries && num_found < num_files; ++i)
    {
        SD_ReadData(&entry, fat->root_start + (uint32_t)(i * sizeof(struct Fat16Entry)), sizeof(struct Fat16Entry));
        if(strncmp(ext, entry.ext, 3) == 0 && GetFileType((unsigned char)entry.filename[0]) == FAT_TYPE_REGULAR)
        {
            Fat_open(fat, &(files[num_found]), entry.filename, entry.ext);
            num_found++;
        }
    }

    return num_found;
}

bool Fat_open(struct FatPartition * fat, struct FatFile * file, char * filename, char * ext)
{
    struct Fat16Entry entry;
    unsigned int i = 0;
    bool found_file = false;

    // Read through and try to find the file
    for(i = 0; i < fat->boot.num_root_entries && !found_file; ++i)
    {
        SD_ReadData(&entry, fat->root_start + (uint32_t)(i * sizeof(struct Fat16Entry)), sizeof(struct Fat16Entry));

        // Check if filename and extension match and if so, grab data
        if(strncmp(ext, entry.ext, 3) == 0 && strncmp(filename, entry.filename, 8) == 0 &&
                GetFileType((unsigned char)entry.filename[0]) == FAT_TYPE_REGULAR)
        {
            strncpy(file->filename, entry.filename, 8);
            strncpy(file->ext, entry.ext, 3);
            file->filesize = entry.filesize;
            file->starting_cluster = entry.starting_cluster;
            file->cur_cluster = file->starting_cluster;
            file->num_clusters = 0;
            file->cur_pos = 0;
            file->part = fat;
            file->type = GetFileType((unsigned char)file->filename[0]);
            
            found_file = true;
        }
    }

    return found_file;
}

uint32_t Fat_read(struct FatFile * file, void * buffer, uint32_t num_bytes)
{
    uint32_t bytes_read = 0;        // How many bytes have been read in this file operation in total
    uint32_t read_num_bytes = 0;    // How many bytes to read for each individual SD_ReadData transaction
    uint32_t file_left, cluster_left;   // Cache each loop iteration how many bytes left in file/cluster

    // Keep reading until we've read the number of requested bytes or hit the end of the file
    while(bytes_read < num_bytes && FILE_BYTES_LEFT(file) > 0)
    {
        file_left = FILE_BYTES_LEFT(file);
        cluster_left = FILE_CLUSTER_LEFT(file);

        // Determine the amount of bytes to read
        read_num_bytes = num_bytes - bytes_read;

        if(read_num_bytes > file_left)
            read_num_bytes = file_left;

        if(read_num_bytes > cluster_left)
            read_num_bytes = cluster_left;

        // Read data into the buffer
        SD_ReadData(buffer + bytes_read, file->part->data_start + ((file->cur_cluster - 2) * file->part->cluster_size) + file->cur_pos, read_num_bytes);

        // Modify byte counters
        file_left -= read_num_bytes;
        cluster_left -= read_num_bytes;
        bytes_read += read_num_bytes;
        file->cur_pos += read_num_bytes;

        // If we've reached the end of the cluster but not the end of the file
        if(cluster_left == 0 && file_left > 0)
        {
            // Grab the next cluster number and reset the current position within that cluster
            SD_ReadData(&(file->cur_cluster), file->part->fat_start + (file->cur_cluster * 2), 2);
            file->cur_pos = 0;
            file->num_clusters++;
        }
    }

    return bytes_read;
}

void Fat_seek(struct FatFile * file, uint32_t amount, enum SeekType type)
{
    uint16_t num_cluster_increase = 0;
    int i = 0;

    switch(type)
    {
        case FAT_SEEK_CUR:
            num_cluster_increase = (uint16_t)((amount + file->cur_pos) / file->part->cluster_size);
            file->num_clusters += num_cluster_increase;
            file->cur_pos = (amount + file->cur_pos) % file->part->cluster_size;
            break;
        case FAT_SEEK_SET:
            num_cluster_increase = (uint16_t)(amount / file->part->cluster_size);
            file->num_clusters = num_cluster_increase;
            file->cur_pos = amount % file->part->cluster_size;
            break;
    }

    // Update the current cluster number based on how many clusters we've increased by
    for(i = 0; i < num_cluster_increase; ++i)
        SD_ReadData(&(file->cur_cluster), file->part->fat_start + (file->cur_cluster * 2), 2);
}

/**
 * Resets all of the pointers in the file back to zero
 * 
 * @param file The file to reset
 */
void ResetFile(struct FatFile * file)
{
    file->cur_cluster = file->starting_cluster;
    file->num_clusters = 0;
    file->cur_pos = 0;
}

/**
 * Determines the type of a file based off the first character in the filename
 * 
 * @param first The first character in the filename
 * 
 * @return What type the file is
 */
static enum FatFileType GetFileType(unsigned char first)
{
    enum FatFileType type = FAT_TYPE_UNUSED;
    
    // Determine the type of the file based on the filename
    switch(first) {
        case 0x00: type = FAT_TYPE_UNUSED; break;
        case 0xE5: type = FAT_TYPE_DELETED; break;
        case 0x05: type = FAT_TYPE_E5; break;
        case 0x2E: type = FAT_TYPE_FOLDER; break;
        default: type = FAT_TYPE_REGULAR;
    }
    
    return type;
}