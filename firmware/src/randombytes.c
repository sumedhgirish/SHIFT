/**
 * @file randombytes.c
 * @author Sumedh Girish
 * @brief Implementation of TRNG-backed random number generation.
 *
 * Interfaces with the MSPM0 hardware True Random Number Generator (TRNG)
 * to provide cryptographically secure random numbers for key generation and
 * nonces.
 */

#include "randombytes.h"
#include "common.h"
#include "ti/driverlib/dl_trng.h"

/**
 * @brief Retrieves a single 32-bit random word from the TRNG hardware.
 *
 * Blocks execution until the TRNG capture register is ready.
 *
 * @return uint32_t A true random 32-bit word.
 */
uint32_t getrandom32(void)
{
    while (!DL_TRNG_isCaptureReady(TRNG))
        ;

    return DL_TRNG_getCapture(TRNG);
}

/**
 * @brief Fills a buffer with true random bytes.
 *
 * Utilizes getrandom32() internally and handles byte alignment.
 */
void randombytes(uint8_t *buf, uint64_t len)
{
    uint8_t capturebuf[4] __attribute__((aligned(4)));
    uint32_t *capture = (uint32_t *) capturebuf;
    uint64_t genBytes = 0;
    while (genBytes < len)
    {
        IF(genBytes % 4 == 0)
        *capture = getrandom32();
        ENDIF

        buf[genBytes] = capturebuf[genBytes % 4];
        genBytes++;
    }
}
