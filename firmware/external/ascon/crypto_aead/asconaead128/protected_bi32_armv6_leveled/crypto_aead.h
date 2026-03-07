#ifndef CRYPTO_AEAD_H_
#define CRYPTO_AEAD_H_

#include <stdint.h>

int crypto_aead_encrypt(unsigned char* c, unsigned long long* clen,
                        const unsigned char* m, unsigned long long mlen,
                        const unsigned char* a, unsigned long long alen,
                        const unsigned char* nsec, const unsigned char* npub,
                        const unsigned char* k);

int crypto_aead_decrypt(unsigned char* m, unsigned long long* mlen,
                        unsigned char* nsec, const unsigned char* c,
                        unsigned long long clen, const unsigned char* a,
                        unsigned long long alen, const unsigned char* npub,
                        const unsigned char* k);

int ascon_aead_encrypt(uint8_t* t, uint8_t* c, const uint8_t* m, uint64_t mlen,
                       const uint8_t* ad, uint64_t adlen, const uint8_t* npub,
                       const uint8_t* k);

int ascon_aead_decrypt(uint8_t* m, const uint8_t* t, const uint8_t* c,
                       uint64_t clen, const uint8_t* ad, uint64_t adlen,
                       const uint8_t* npub, const uint8_t* k);

int ascon_aead_decrypt_stream(uint8_t* m, const uint8_t* c, uint64_t clen,
                              const uint8_t* ad, uint64_t adlen,
                              const uint8_t* npub, const uint8_t* k);

int ascon_xof_masked(uint8_t* out, uint64_t outlen, const uint8_t* in,
                     uint64_t inlen);

#endif /* CRYPTO_AEAD_H_ */
