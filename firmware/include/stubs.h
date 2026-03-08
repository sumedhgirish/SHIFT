/**
 * @file stubs.h
 * @author Sumedh Girish
 * @brief Secure Memory and Timing-Safe Operations.
 *
 * Provides utility functions for secure memory management and timing-safe
 * comparisons to protect against side-channel attacks. These routines are
 * critical for preventing information leakage during cryptographic operations.
 */

#ifndef __STUBS_H__
#define __STUBS_H__

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Secure memory comparison using domain-separated hashing.
 *
 * Instead of a direct byte-by-byte comparison (which is susceptible to timing
 * attacks), this function hashes both inputs using Ascon-XOF and compares the
 * resulting fixed-length digests. This ensures that the execution time is
 * independent of how many prefix bytes match, providing strong protection
 * against timing-based side-channel analysis.
 *
 * @param a First memory block to compare.
 * @param b Second memory block to compare.
 * @param size Number of bytes to compare from each block.
 * @return bool true if the digests (and thus the original data) are identical.
 */
bool securecmp(const uint8_t *a, const uint8_t *b, uint32_t size);

bool checkpin(uint8_t inputPin[6]);

/**
 * @brief Securely clears memory context using volatile-safe operations.
 *
 * Implements a robust and high-performance memory clearing strategy:
 * 1. Byte-by-byte head cleanup until word alignment is reached.
 * 2. High-speed 32-bit word-level clearing for the aligned core.
 * 3. Byte-by-byte tail cleanup for any remaining bytes.
 *
 * This function uses 'volatile' pointers to ensure that the compiler does NOT
 * optimize away the memory write (which often happens with standard memset
 * when the buffer is about to be freed or go out of scope), which is essential
 * for ensuring sensitive keys are actually wiped from SRAM.
 *
 * @param v Pointer to the memory region to clear.
 * @param n Length of the memory region in bytes.
 * @param value Byte value to fill (typically 0x00).
 */
void memclear(void *v, uint32_t n, uint8_t value);

#endif /* __STUBS_H__ */
