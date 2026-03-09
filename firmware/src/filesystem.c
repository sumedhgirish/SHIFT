#include "filesystem.h"
#include "flash.h"
#include "stubs.h"
#include <assert.h>
#include <string.h>

volatile FS_Stage stage;

__attribute__((section(".fat"))) // Link into the location of the FAT in flash
volatile const FAT_TableTypeFlash FAT_Table;

__attribute__((section(".metadata"))) // Link into metadata in flash
volatile const FS_MetadataEntryFlash Metadata_Table[NUM_SLOTS];

__attribute__((section(".filedata"))) // Link into filedata in flash
volatile const FS_FiledataFlash Filedata_Table;

__attribute__((section(".systemconf"))) // Link into filedata in flash
volatile const FS_SystemStatusFlash SystemStatus;

void LoadFile(uint8_t slot, StatusCode *status)
{
    if (slot >= NUM_SLOTS)
    {
        *status = INVALIDSLOT;
        return;
    }

    if (Metadata_Table[slot].status.magic != 0xdeadf00d ||
        securecmp((uint8_t *) Metadata_Table[slot].status.id,
                  (uint8_t *) FAT_Table.entry[slot].uuid, 16) == false)
    {
        *status = INVALIDSLOT;
        return;
    }

    memcpy((uint8_t *) &stage.as.file.entry, (uint8_t *) &FAT_Table.entry[slot],
           sizeof(FS_FileEntry));

    memcpy((uint8_t *) &stage.as.file.metadata,
           (uint8_t *) &Metadata_Table[slot].data, sizeof(FS_Metadata));

    memcpy((uint8_t *) &stage.as.file.content,
           (uint8_t *) Filedata_Table.data[slot], MAX_FILE_SIZE);
}

void StoreFile(uint8_t slot, StatusCode *status)
{
    if (slot >= NUM_SLOTS)
    {
        *status = INVALIDSLOT;
        return;
    }

    FLASH_Write((uint32_t) Filedata_Table.data[slot],
                (uint8_t *) stage.as.file.content, MAX_FILE_SIZE, status);

    FS_MetadataEntryFlash metadata_entry;

    metadata_entry.data = stage.as.file.metadata;
    memcpy(metadata_entry.status.id, (uint8_t *) &stage.as.file.entry.uuid, 16);
    metadata_entry.status.magic = 0xdeadf00d;

    FLASH_Write((uint32_t) &Metadata_Table[slot].asBytes,
                metadata_entry.asBytes, sizeof(FS_MetadataEntryFlash), status);

    FLASH_Write((uint32_t) &FAT_Table.entry[slot],
                (uint8_t *) &stage.as.file.entry, sizeof(FS_FileEntry), status);
    if (*status == FLASHWRITEERROR || *status == FLASHERASEERROR)
        return;
}
