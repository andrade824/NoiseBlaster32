/* 
 * File:   fat.h
 * Author: Devon
 *
 * Created on September 8, 2015, 8:43 PM
 */

#ifndef FAT_H
#define	FAT_H

#include <stdint.h>
#include <stdbool.h>

// Maximum number of partitions a master boot record can have (assuming no extended partitions)
#define MAX_MBR_PARTITIONS 4

// Different parameters for each partition on the disk
struct PartitionTable {
    uint8_t first_byte;
    uint8_t start_chs[3];
    uint8_t partition_type;
    uint8_t end_chs[3];
    uint32_t start_sector;
    uint32_t length_sectors;
} __attribute((packed));

// Each partition starts off with a boot sector that describes it
struct Fat16BootSector {
    uint8_t jump[3];
    char oem[8];
    uint16_t sector_size;
    uint8_t num_sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t num_fats;
    uint16_t num_root_entries;
    uint16_t total_sectors_short;   // Zero for disks/partitions bigger than 32MiB (use total_num_sectors)
    uint8_t media_descriptor;
    uint16_t fat_num_sectors;   // Num sectors used for one FAT table, N/A for FAT32
    uint16_t chs_sectors_per_track;
    uint16_t chs_num_heads;
    uint32_t num_hidden_sectors;    // Num sectors before boot sector, maybe zero
    uint32_t total_num_sectors;     // For disks/partitions bigger than 32MiB
    uint8_t drive_number;
    uint8_t current_head;
    uint8_t boot_sig;       // The value 0x29 indicates the three next fields are valid
    uint32_t volume_id;
    char volume_label[11];
    char filesystem_type[8];
    uint8_t boot_code[448];
    uint16_t boot_sector_sig;   // Must be 0x55AA
} __attribute((packed));

// Represents a single FAT Partition (partition data and the boot sector)
struct FatPartition {
    uint8_t partition_type;
    uint32_t start_sector;
    uint32_t cluster_size;  // In bytes
    uint32_t fat_start;     // Number of bytes from beginning of disk until the first FAT table
    uint32_t root_start;    // Number of bytes from beginning of disk until the root entries
    uint32_t data_start;    // Number of bytes from beginning of disk until the data section begins
    struct Fat16BootSector boot; // Information from the boot sector
};

// A single file/directory entry in FAT16 raw format
struct Fat16Entry {
    char filename[8];
    char ext[3];
    uint8_t attributes;
    uint8_t reserved[10];
    uint16_t timestamp;
    uint16_t datestamp;
    uint16_t starting_cluster;
    uint32_t filesize;  // In bytes
} __attribute((packed));

// Represents a file in the FAT partition
struct FatFile {
    char filename[8];
    char ext[3];
    uint16_t starting_cluster;
    uint16_t cur_cluster;   // The cluster number of the cluster we're currently reading from
    uint16_t num_clusters;   // How many clusters we've read through so far
    uint32_t cur_pos;   // Position within the current cluster in bytes
    uint32_t filesize;  // In bytes
    struct FatPartition * part; // Pointer to the partition this file is in
    // TODO: Add file type (unused, deleted, starts_e5, directory, regular)
};

enum SeekType {FAT_SEEK_CUR, FAT_SEEK_SET};

// Takes in a pointer to a FatFile and returns how many bytes have currently been read/are left
#define FILE_BYTES_READ(file) ((file->num_clusters * file->part->cluster_size) + file->cur_pos)
#define FILE_BYTES_LEFT(file) (file->filesize - FILE_BYTES_READ(file))

// Takes in a pointer to a file and returns how many bytes are left in the current cluster
#define FILE_CLUSTER_LEFT(file) (file->part->cluster_size - file->cur_pos)

// Function prototypes
bool OpenFirstFatPartition(struct FatPartition * fat);
uint16_t GetFilesByExt(struct FatPartition * fat, struct FatFile * files, uint16_t num_files, char * ext);
bool Fat_open(struct FatPartition * fat, struct FatFile * file, char * filename, char * ext);
uint32_t Fat_read(struct FatFile * file, void * buffer, uint32_t num_bytes);
void Fat_seek(struct FatFile * file, uint32_t amount, enum SeekType type);

#endif	/* FAT_H */

