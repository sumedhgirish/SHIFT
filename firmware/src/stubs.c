/**
 * @file stubs.c
 * @author Sumedh Girish
 * @brief Implementation of Secure Memory and Side-Channel Protection routines.
 *
 * Provides specialized memory utilities designed to withstand side-channel
 * attacks and ensure compiler-compliant volatile memory clearing.
 */

#include "stubs.h"
#include "ascon.h"
#include "common.h"
#include "filesystem.h"
#include "secrets.h"
#include "ti/driverlib/dl_flashctl.h"
#include <stddef.h>

#define RAMFUNC                                                                \
    __attribute__((section(".TI.ramfunc"))) __attribute__((noinline))

/**
 * @brief Constant-time sensitive memory comparison using Ascon-XOF.
 *
 * Instead of comparing bytes directly (which is vulnerable to timing attacks),
 * this function hashes both inputs into a temporary buffer and then performs
 * a bitwise accumulated XOR comparison on the hashes.
 */
bool securecmp(const uint8_t *a, const uint8_t *b, uint32_t size)
{
    uint8_t ahash[CRYPTO_ABYTES] = {0};
    uint8_t bhash[CRYPTO_ABYTES] = {0};

    ascon_xof_masked(ahash, CRYPTO_ABYTES, a, size);
    ascon_xof_masked(bhash, CRYPTO_ABYTES, b, size);

    uint8_t result = 0;
    for (uint32_t i = 0; i < CRYPTO_ABYTES; ++i)
        result |= ahash[i] ^ bhash[i];

    memclear(ahash, CRYPTO_ABYTES, 0xFF);
    memclear(bhash, CRYPTO_ABYTES, 0xFF);

    return result == 0;
}

/**
 * @brief Securely validates a user-provided PIN against the stored system PIN.
 *
 * Executes both a standard memory comparison and a secure, constant-time
 * hash-based comparison (`securecmp()`). Any discrepancy immediately
 * corrupts the timeout token in flash memory to trip the internal trap system,
 * permanently mitigating brute force attempts.
 */
bool checkpin(uint8_t inputPin[6])
{
    uint64_t storedPin = 0;
    memcpy(&storedPin, (const uint8_t *) HSM_PIN, 6);

    uint64_t inputPinVal = 0;
    memcpy(&inputPinVal, inputPin, 6);

    bool valid1 = storedPin == inputPinVal;
    bool valid2 =
        securecmp((const uint8_t *) HSM_PIN, (const uint8_t *) inputPin, 6);

    uint64_t value = (0 - (uint64_t) (valid1 && valid2));
    DL_FlashCTL_programMemoryBlocking64WithECCGenerated(
        FLASHCTL, (uint32_t) &SystemStatus.timeout, (uint32_t *) &value, 2,
        DL_FLASHCTL_REGION_SELECT_MAIN);

    return valid1 && valid2;
}

/**
 * @brief Volatile memory clearing/setting to prevent compiler optimization.
 *
 * Standard memset() may be optimized away by compilers if the buffer is not
 * read again before it goes out of scope. memclear() uses volatile pointers
 * and a three-stage clear (alignment, word-clear, tail-clear) to ensure that
 * the memory is actually modified in SRAM, protecting against data remanence.
 */
RAMFUNC void memclear(void *v, uint32_t n, uint8_t value)
{
    IF(v == NULL || n == 0)
    return;
    ENDIF

    volatile uint8_t *p = (volatile uint8_t *) v;

    // 1. Align pointer to 4-byte boundary (Byte-by-byte)
    while (n > 0 && ((uintptr_t) p & 3) != 0)
    {
        *p++ = value;
        n--;
    }

    // 2. High-speed word clear (32-bit at a time)
    volatile uint32_t *pw;
    if (n >= 4)
    {
        uint32_t word_val = (uint32_t) value | ((uint32_t) value << 8) |
                            ((uint32_t) value << 16) | ((uint32_t) value << 24);
        pw = (volatile uint32_t *) (uintptr_t) p;
        while (n >= 4)
        {
            *pw++ = word_val;
            n -= 4;
        }
        p = (volatile uint8_t *) pw;
    }

    // 3. Final Tail cleanup (Byte-by-byte)
    while (n > 0)
    {
        *p++ = value;
        n--;
    }
}
