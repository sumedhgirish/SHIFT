/**
 * @file flash.h
 * @author Sumedh Girish
 * @brief Low-level Flash Memory Controller Driver for MSPM0.
 *
 * This module provides a thread-safe (via polling) abstraction for flash
 * programming on the TI MSPM0L2228. It implements critical safety features
 * including:
 * 1. mandatory SRAM execution for erase/write routines (RAMFUNC).
 * 2. Automatic sector-level unprotection/re-protection.
 * 3. Hardware-software synchronization to prevent bus contention hangs.
 */

#ifndef __FLASH_H__
#define __FLASH_H__

#include "status.h"
#include "ti/driverlib/dl_flashctl.h"
#include <stdint.h>

/**
 * @brief Size of a single physical flash page/sector on the MSPM0.
 */
#define FLASH_PAGE_SIZE DL_FLASHCTL_SECTOR_SIZE

/**
 * @brief Macro to place functions in the .TI.ramfunc section.
 *
 * On the MSPM0, the flash controller cannot fetch instructions from the flash
 * bank it is currently modifying. Functions tagged with RAMFUNC are relocated
 * to SRAM by the linker/startup code to allow the CPU to continue execution
 * during flash write cycles.
 */
#define RAMFUNC                                                                \
    __attribute__((section(".TI.ramfunc"))) __attribute__((noinline))

/**
 * @brief Erases a physical sector of flash memory.
 *
 * Temporarily disables flash protection for the target sector, issues the erase
 * command to the controller, and re-engages protection. Safe to call only
 * when executing from SRAM.
 *
 * @param address Base address of the 1024-byte sector to erase.
 * @param status Global status variable to update on failure.
 *
 * @note This function blocks until the erase cycle completes.
 */
RAMFUNC void FLASH_Erase(uint32_t address, StatusCode *status);

/**
 * @brief Writes data into a physical flash sector.
 *
 * Writes a buffer into flash memory in 64-bit words. Handles flash unlocking,
 * execution of the write sequence, and locking. Automatically pads unaligned
 * writes to 64-bit boundaries with 0xFF.
 *
 * @param address Base address of the flash memory to write to.
 * @param buffer Pointer to the source data in SRAM.
 * @param size Number of bytes to write.
 * @param status Global status variable to update on failure/misalignment.
 *
 * @note Must execute from SRAM.
 */
RAMFUNC void FLASH_Write(uint32_t address, uint8_t *buffer, uint32_t size,
                         StatusCode *status);

#endif /* __FLASH_H__ */
