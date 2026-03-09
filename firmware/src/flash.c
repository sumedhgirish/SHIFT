
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

static RAMFUNC void FLASH_ReadSector(uint32_t address, uint32_t *buffer)
{
    const volatile uint32_t *src = (const uint32_t *) address;
    for (uint32_t i = 0U; i < DL_FLASHCTL_SECTOR_SIZE / 4U; i++)
    {
        buffer[i] = src[i];
    }
}

static RAMFUNC void FLASH_ProcessSector(uint32_t sector_base, uint32_t offset,
                                        uint32_t chunk, const uint8_t *src_ptr,
                                        StatusCode *status)
{
    FLASH_ReadSector(sector_base, sector_buffer);

    bool needs_update = false;
    bool is_blank = true;
    uint8_t *dst = (uint8_t *) sector_buffer;

    for (uint32_t i = 0; i < DL_FLASHCTL_SECTOR_SIZE; i++)
    {
        if (dst[i] != 0xFF)
            is_blank = false;

        if (i >= offset && i < offset + chunk)
        {
            uint8_t newVal = src_ptr[i - offset];
            if (dst[i] != newVal)
            {
                dst[i] = newVal;
                needs_update = true;
            }
        }
    }

    if (needs_update)
    {
        if (!is_blank)
        {
            FLASH_Erase(sector_base, status);
            if (*status == FLASHERASEERROR)
                return;
        }

        DL_FlashCTL_executeClearStatus(FLASHCTL);
        DL_FlashCTL_unprotectSector(FLASHCTL, sector_base,
                                    DL_FLASHCTL_REGION_SELECT_MAIN);
        if (DL_FlashCTL_programMemoryBlockingFromRAM64WithECCGenerated(
                FLASHCTL, sector_base, sector_buffer,
                DL_FLASHCTL_SECTOR_SIZE / 4U, DL_FLASHCTL_REGION_SELECT_MAIN) !=
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

        FLASH_ProcessSector(sector_base, offset, chunk, src_ptr, status);
        if (*status == FLASHWRITEERROR || *status == FLASHERASEERROR)
            return;

        remaining -= chunk;
        current_addr += chunk;
        src_ptr += chunk;
    }
}
