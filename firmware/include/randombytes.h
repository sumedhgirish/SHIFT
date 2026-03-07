/**
 * @file randombytes.h
 * @author Sumedh Girish
 * @brief Hardware-backed Random Number Generation.
 *
 * Provides an interface for the MSPM0 True Random Number Generator (TRNG)
 * peripheral. Used for generating nonces, session keys, and randomized
 * filesystem metadata.
 */

#ifndef __RANDOMBYTES_H__
#define __RANDOMBYTES_H__

#include <stdint.h>

/**
 * @brief Retrieves a 32-bit random word from the hardware TRNG.
 *
 * Blocks indefinitely until the TRNG capture register is ready with new
 * entropy. The TRNG must be properly initialized in the system configuration.
 *
 * @return uint32_t A fresh random 32-bit unsigned integer.
 */
uint32_t getrandom32(void);

/**
 * @brief Fills a buffer with random bytes.
 *
 * Populates the destination buffer using entropy from the 32-bit hardware TRNG.
 * Handles fractional words and byte-level alignment internally to ensure the
 * entire buffer is filled.
 *
 * @param buf Destination buffer to fill with random data.
 * @param len Total number of bytes to generate (up to 2^64-1).
 */
void randombytes(uint8_t *buf, uint64_t len);

#endif /* __RANDOMBYTES_H__ */
