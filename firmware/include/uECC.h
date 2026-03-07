/* Copyright 2014, Kenneth MacKay. Licensed under the BSD 2-clause license. */

/**
 * @file uECC.h
 * @author Kenneth MacKay (refined by Sumedh Girish)
 * @brief Micro-ECC Library for ECDH and ECDSA.
 *
 * This version of Micro-ECC is used for all asymmetric cryptographic operations
 * in the SHIFT firmware, specifically targeting NIST P-256 for secure key
 * exchange and message signing.
 */

#ifndef _MICRO_ECC_H_
#define _MICRO_ECC_H_

#include <stdint.h>

/** @brief Other architecture selection value. */
#define uECC_arch_other 0
/** @brief x86 architecture selection value. */
#define uECC_x86 1
/** @brief x86_64 architecture selection value. */
#define uECC_x86_64 2
/** @brief ARM architecture selection value. */
#define uECC_arm 3
/** @brief ARM Thumb architecture selection value. */
#define uECC_arm_thumb 4
/** @brief AVR architecture selection value. */
#define uECC_avr 5
/** @brief ARM Thumb-2 architecture selection value. */
#define uECC_arm_thumb2 6

/** @brief Use standard C99 implementation only. */
#define uECC_asm_none 0
/** @brief Use GCC inline assembly optimized for minimum size. */
#define uECC_asm_small 1
/** @brief Use GCC inline assembly optimized for maximum speed. */
#define uECC_asm_fast 2

#ifndef uECC_ASM
/** @brief Selected assembly optimization level. */
#define uECC_ASM uECC_asm_fast
#endif

/** @brief SECP160R1 Curve ID. */
#define uECC_secp160r1 1
/** @brief SECP192R1 Curve ID. */
#define uECC_secp192r1 2
/** @brief SECP256R1 (NIST P-256) Curve ID. */
#define uECC_secp256r1 3
/** @brief SECP256K1 Curve ID. */
#define uECC_secp256k1 4
/** @brief SECP224R1 Curve ID. */
#define uECC_secp224r1 5

#ifndef uECC_CURVE
/** @brief Selected elliptic curve for the project. */
#define uECC_CURVE uECC_secp256r1
#endif

#ifndef uECC_SQUARE_FUNC
/** @brief Enable optimized scalar squaring (8% faster, more code). */
#define uECC_SQUARE_FUNC 1
#endif

#define uECC_CONCAT1(a, b) a##b
#define uECC_CONCAT(a, b) uECC_CONCAT1(a, b)

#define uECC_size_1 20 /**< Byte size for secp160r1. */
#define uECC_size_2 24 /**< Byte size for secp192r1. */
#define uECC_size_3 32 /**< Byte size for secp256r1 (P-256). */
#define uECC_size_4 32 /**< Byte size for secp256k1. */
#define uECC_size_5 28 /**< Byte size for secp224r1. */

/** @brief Number of bytes required for a single coordinate/scalar on the
 * selected curve. */
#define uECC_BYTES uECC_CONCAT(uECC_size_, uECC_CURVE)

/**
 * @brief RNG function type.
 *
 * The RNG function should fill 'size' random bytes into 'dest'. It should
 * return 1 on success, or 0 on failure.
 */
typedef int (*uECC_RNG_Function)(uint8_t *dest, unsigned size);

/**
 * @brief Sets the function that will be used to generate random bytes.
 *
 * On embedded platforms, this MUST be called before any key generation or
 * signing if true randomness is required.
 *
 * @param rng_function The function that will be used to generate random bytes.
 */
void uECC_set_rng(uECC_RNG_Function rng_function);

/**
 * @brief Create a public/private key pair.
 *
 * @param[out] public_key Will be filled with the public key (2*uECC_BYTES
 * bytes).
 * @param[out] private_key Will be filled with the private key (uECC_BYTES
 * bytes).
 * @return int 1 if the key pair was generated successfully, 0 otherwise.
 */
int uECC_make_key(uint8_t public_key[uECC_BYTES * 2],
                  uint8_t private_key[uECC_BYTES]);

/**
 * @brief Compute a shared secret given your secret key and someone else's
 * public key (ECDH).
 *
 * Note: It is recommended that you hash the result before using it for
 * symmetric encryption.
 *
 * @param[in] public_key The public key of the remote party.
 * @param[in] private_key Your private key.
 * @param[out] secret Will be filled with the shared secret value (uECC_BYTES
 * bytes).
 * @return int 1 if the shared secret was generated successfully, 0 otherwise.
 */
int uECC_shared_secret(const uint8_t public_key[uECC_BYTES * 2],
                       const uint8_t private_key[uECC_BYTES],
                       uint8_t secret[uECC_BYTES]);

/**
 * @brief Generate an ECDSA signature for a given hash value.
 *
 * @param[in] private_key Your private key.
 * @param[in] message_hash The hash of the message to sign (already computed).
 * @param[out] signature Will be filled with the signature (2*uECC_BYTES bytes).
 * @return int 1 if the signature generated successfully, 0 otherwise.
 */
int uECC_sign(const uint8_t private_key[uECC_BYTES],
              const uint8_t message_hash[uECC_BYTES],
              uint8_t signature[uECC_BYTES * 2]);

/**
 * @brief Interface for passing arbitrary hash functions to uECC.
 */
typedef struct uECC_HashContext
{
    /** @brief Initialize the hash state. */
    void (*init_hash)(struct uECC_HashContext *context);
    /** @brief Feed data into the hash. */
    void (*update_hash)(struct uECC_HashContext *context,
                        const uint8_t *message, unsigned message_size);
    /** @brief Extract the final digest. */
    void (*finish_hash)(struct uECC_HashContext *context, uint8_t *hash_result);
    unsigned block_size;  /**< Hash function internal block size in bytes. */
    unsigned result_size; /**< Hash function final digest size in bytes. */
    uint8_t *tmp; /**< Working buffer of at least (2 * result_size + block_size)
                     bytes. */
} uECC_HashContext;

/**
 * @brief Generate an ECDSA signature using a deterministic algorithm (RFC
 * 6979).
 *
 * @param[in] private_key Your private key.
 * @param[in] message_hash The hash of the message to sign.
 * @param[in] hash_context A hash context for HMAC-DRBG operations.
 * @param[out] signature Will be filled with the signature value.
 * @return int 1 if the signature generated successfully, 0 otherwise.
 */
int uECC_sign_deterministic(const uint8_t private_key[uECC_BYTES],
                            const uint8_t message_hash[uECC_BYTES],
                            uECC_HashContext *hash_context,
                            uint8_t signature[uECC_BYTES * 2]);

/**
 * @brief Verify an ECDSA signature against a public key and hash.
 *
 * @param[in] public_key The signer's public key.
 * @param[in] hash The hash of the signed data.
 * @param[in] signature The signature value.
 * @return int 1 if the signature is valid, 0 if it is invalid or an error
 * occurred.
 */
int uECC_verify(const uint8_t public_key[uECC_BYTES * 2],
                const uint8_t hash[uECC_BYTES],
                const uint8_t signature[uECC_BYTES * 2]);

/**
 * @brief Compress a public key (Y coordinate is reduced to parity bit).
 *
 * @param[in] public_key The public key to compress.
 * @param[out] compressed Will be filled with the compressed key (uECC_BYTES + 1
 * bytes).
 */
void uECC_compress(const uint8_t public_key[uECC_BYTES * 2],
                   uint8_t compressed[uECC_BYTES + 1]);

/**
 * @brief Decompress a compressed public key.
 *
 * @param[in] compressed The compressed public key.
 * @param[out] public_key Will be filled with the decompressed key (2 *
 * uECC_BYTES bytes).
 */
void uECC_decompress(const uint8_t compressed[uECC_BYTES + 1],
                     uint8_t public_key[uECC_BYTES * 2]);

/**
 * @brief Check if a public key is a valid point on the curve.
 *
 * Includes checks for the point at infinity and whether the point is on the
 * curve.
 *
 * @param[in] public_key The public key to check.
 * @return int 1 if the public key is valid, 0 otherwise.
 */
int uECC_valid_public_key(const uint8_t public_key[uECC_BYTES * 2]);

/**
 * @brief Compute the corresponding public key for a given private key.
 *
 * @param[in] private_key The private key.
 * @param[out] public_key Will be filled with the corresponding public key.
 * @return int 1 if the key was computed successfully, 0 otherwise.
 */
int uECC_compute_public_key(const uint8_t private_key[uECC_BYTES],
                            uint8_t public_key[uECC_BYTES * 2]);

/**
 * @brief Returns the value of uECC_BYTES for the current build.
 * @return int Current uECC_BYTES value.
 */
int uECC_bytes(void);

/**
 * @brief Returns the value of uECC_CURVE for the current build.
 * @return int Current uECC_CURVE value.
 */
int uECC_curve(void);

#endif /* _MICRO_ECC_H_ */
