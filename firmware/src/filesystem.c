/**
 * @file filesystem.c
 * @author Sumedh Girish
 * @brief Flash Filesystem Implementation and Storage Tables.
 *
 * Implements the core flash storage tables linked directly into designated
 * memory segments via the linker script. Provides load/store abstractions
 * that interact safely with the cryptographic staging area.
 */

#include "filesystem.h"
#include "common.h"
#include "flash.h"
#include "stubs.h"
#include <assert.h>
#include <string.h>

/** @brief Staging area in SRAM for cryptographic ops before syncing to Flash.
 */
volatile FS_Stage stage;

/** @brief File Allocation Table, mapped to fixed flash section. */
__attribute__((section(".fat"))) // Link into the location of the FAT in flash
volatile const FAT_TableTypeFlash FAT_Table;

/** @brief File metadata array, mapped to fixed flash section. */
__attribute__((section(".metadata"))) // Link into metadata in flash
volatile const FS_MetadataEntryFlash Metadata_Table[NUM_SLOTS];

/** @brief File content storage blocks, mapped to fixed flash section. */
__attribute__((section(".filedata"))) // Link into filedata in flash
volatile const FS_FiledataFlash Filedata_Table;

/** @brief System runtime configuration state, mapped to fixed flash section. */
__attribute__((section(".systemconf"))) // Link into filedata in flash
volatile const FS_SystemStatusFlash SystemStatus;

void LoadFile(uint8_t slot, StatusCode *status)
{
    IF(slot >= NUM_SLOTS)
    {
        *status = INVALIDSLOT;
        return;
    }
    ENDIF

    IF(Metadata_Table[slot].status.magic != 0xdeadf00d ||
       securecmp((uint8_t *) Metadata_Table[slot].status.id,
                 (uint8_t *) FAT_Table.entry[slot].uuid, 16) == false)
    {
        *status = INVALIDSLOT;
        return;
    }
    ENDIF

    memcpy((uint8_t *) &stage.as.file.entry, (uint8_t *) &FAT_Table.entry[slot],
           sizeof(FS_FileEntry));

    memcpy((uint8_t *) &stage.as.file.metadata,
           (uint8_t *) &Metadata_Table[slot].data, sizeof(FS_Metadata));

    memcpy((uint8_t *) &stage.as.file.content,
           (uint8_t *) Filedata_Table.data[slot], MAX_FILE_SIZE);
}

void StoreFile(uint8_t slot, StatusCode *status)
{
    IF(slot >= NUM_SLOTS)
    {
        *status = INVALIDSLOT;
        return;
    }
    ENDIF

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
    IF(*status == FLASHWRITEERROR || *status == FLASHERASEERROR)
    return;
    ENDIF
}
