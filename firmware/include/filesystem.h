#ifndef __FILESYSTEM_H__
#define __FILESYSTEM_H__

#include "secrets.h"
#include "ti/driverlib/dl_flashctl.h"
#include <stdint.h>

#ifndef PACKED
#define PACKED __attribute__((packed))
#endif

#define RAMFUNC                                                                \
    __attribute__((section(".TI.ramfunc"))) __attribute__((noinline))

typedef struct
{
    uint8_t key[ECC_PUB_KEYSIZE];
    uint8_t nonce[NONCE_SIZE];
    uint8_t mesgid[ID_SIZE];
    uint8_t tag[ASCON_TAG_SIZE];
} PACKED FS_Preamble;

#define MAX_FILE_SIZE 8192
typedef uint8_t FS_FileData[MAX_FILE_SIZE];

typedef struct
{
    uint16_t groupid;
    uint8_t key[ECC_PUB_KEYSIZE];
    uint8_t nonce[NONCE_SIZE];
    uint8_t fileid[ID_SIZE];
    uint8_t filename[32];
    uint8_t tag[ASCON_TAG_SIZE];
} PACKED FS_Metadata;

typedef struct
{
    uint8_t uuid[16];
    uint16_t filesize;
    uint16_t _pad;
    uint32_t fileaddr;
} PACKED FS_FileEntry;

typedef struct
{
    FS_Metadata metadata;
    FS_FileEntry entry;
    FS_FileData content;
} PACKED FS_File;

#define NUM_SLOTS 8

typedef struct
{
    uint8_t slot;
    uint16_t groupid;
    uint8_t filename[32];
} PACKED FS_Slot;

typedef struct
{
    uint32_t numEntries;
    FS_Slot slots[NUM_SLOTS];
} PACKED FS_FAT;

typedef struct
{
    FS_Preamble preamble;
    union
    {
        FS_File file;
        FS_FAT fat;
    } as;
} PACKED FS_Stage;

extern volatile FS_Stage stage;

typedef union
{
    uint8_t asBytes[DL_FLASHCTL_SECTOR_SIZE];
    FS_FileEntry entry[NUM_SLOTS];
} FAT_TableTypeFlash;

extern volatile const FAT_TableTypeFlash FAT_Table;

typedef union
{
    uint8_t asBytes[DL_FLASHCTL_SECTOR_SIZE];
    FS_Metadata data;
    struct
    {
        uint8_t _padding[512];
        uint8_t id[16];
        uint32_t magic;
    } status;
} FS_MetadataEntryFlash;

extern volatile const FS_MetadataEntryFlash Metadata_Table[NUM_SLOTS];

typedef struct
{
    uint8_t data[NUM_SLOTS][8][DL_FLASHCTL_SECTOR_SIZE];
} FS_FiledataFlash;

extern volatile const FS_FiledataFlash Filedata_Table;

typedef struct
{
    bool timeout;
} FS_SystemStatusFlash;

extern volatile const FS_SystemStatusFlash SystemStatus;

void LoadFile(uint8_t slot, StatusCode *status);
void StoreFile(uint8_t slot, StatusCode *status);

#endif
