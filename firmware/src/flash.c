/**
 * @file flash.c
 * @author Sumedh Girish
 * @brief Flash Memory Controller Implementation.
 *
 * Provides safe routines for flash sector erase and program operations.
 * Operations are strictly constrained to SRAM execution spaces via `RAMFUNC`.
 */

#include "flash.h"
#include "status.h"
#include "ti/devices/msp/m0p/mspm0l222x.h"
#include "ti/driverlib/dl_flashctl.h"
#include <stdbool.h>
#include <stddef.h>

__attribute__((aligned(8))) // Make this a valid 64 bit address
static uint32_t sector_buffer[DL_FLASHCTL_SECTOR_SIZE / 4U];

RAMFUNC void FLASH_Erase(uint32_t address, StatusCode *status)
{
    DL_FlashCTL_executeClearStatus(FLASHCTL);
    DL_FlashCTL_unprotectSector(FLASHCTL, address,
                                DL_FLASHCTL_REGION_SELECT_MAIN);
    if (DL_FlashCTL_eraseMemoryFromRAM(FLASHCTL, address,
                                       DL_FLASHCTL_COMMAND_SIZE_SECTOR) !=
        DL_FLASHCTL_COMMAND_STATUS_PASSED)
    {
        *status = FLASHERASEERROR;
        return;
    }

    if (DL_FlashCTL_waitForCmdDone(FLASHCTL) == false)
        *status = FLASHERASEERROR;
}

/**
 * @brief Reads a full flash sector into a 64-bit aligned SRAM buffer.
 *
 * Essential for the read-modify-write cycle. Executes from SRAM to 
 * avoid bus contention during subsequent flash erase commands.
 *
 * @param address Base address of the sector to read.
 * @param buffer Pointer to the 64-bit aligned SRAM buffer.
 */
static RAMFUNC void FLASH_ReadSector(uint32_t address, uint32_t *buffer)
{
    const volatile uint32_t *src = (const uint32_t *) address;
    for (uint32_t i = 0U; i < DL_FLASHCTL_SECTOR_SIZE / 4U; i++)
    {
        buffer[i] = src[i];
    }
}

/**
 * @brief Executes a read-modify-write cycle for a single flash sector.
 *
 * If the data chunk doesn't cleanly overwrite the entire sector, the current
 * sector is loaded into SRAM, patched, erased, and reprogrammed.
 *
 * @param sector_base Aligned flash sector base address.
 * @param offset Byte-offset within the sector to apply the new data.
 * @param chunk Size of the new data in bytes.
 * @param src_ptr Pointer to the new data in SRAM.
 * @param status Global status variable.
 * @param fullChunk True if the chunk size precisely matches the sector size.
 */
static RAMFUNC void FLASH_ProcessSector(uint32_t sector_base, uint32_t offset,
                                        uint32_t chunk, const uint8_t *src_ptr,
                                        StatusCode *status, bool fullChunk)
{
    if (!fullChunk)
        FLASH_ReadSector(sector_base, sector_buffer);

    uint8_t *dst = (uint8_t *) sector_buffer;
    for (uint32_t i = 0; i < chunk; i++)
    {
        dst[i + offset] = src_ptr[i];
    }

    FLASH_Erase(sector_base, status);
    if (*status == FLASHERASEERROR)
        return;

    DL_FlashCTL_executeClearStatus(FLASHCTL);
    DL_FlashCTL_unprotectSector(FLASHCTL, sector_base,
                                DL_FLASHCTL_REGION_SELECT_MAIN);
    if (DL_FlashCTL_programMemoryBlockingFromRAM64WithECCGenerated(
            FLASHCTL, sector_base, sector_buffer, DL_FLASHCTL_SECTOR_SIZE / 4U,
            DL_FLASHCTL_REGION_SELECT_MAIN) !=
        DL_FLASHCTL_COMMAND_STATUS_PASSED)
    {
        *status = FLASHWRITEERROR;
        return;
    }

    if (DL_FlashCTL_waitForCmdDone(FLASHCTL) == false)
    {
        *status = FLASHWRITEERROR;
        return;
    }
}

RAMFUNC void FLASH_Write(uint32_t address, uint8_t *buffer, uint32_t size,
                         StatusCode *status)
{
    uint32_t remaining = size;
    uint32_t current_addr = address;
    const uint8_t *src_ptr = buffer;

    while (remaining > 0U)
    {
        uint32_t sector_base =
            current_addr & ~(uint32_t) (DL_FLASHCTL_SECTOR_SIZE - 1U);
        uint32_t offset = current_addr % DL_FLASHCTL_SECTOR_SIZE;
        uint32_t chunk = DL_FLASHCTL_SECTOR_SIZE - offset;

        if (chunk > remaining)
            chunk = remaining;

        FLASH_ProcessSector(sector_base, offset, chunk, src_ptr, status,
                            chunk == DL_FLASHCTL_SECTOR_SIZE);
        if (*status == FLASHWRITEERROR || *status == FLASHERASEERROR)
            return;

        remaining -= chunk;
        current_addr += chunk;
        src_ptr += chunk;
    }
}
