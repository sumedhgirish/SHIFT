#include "crypto_aead.h"

#include <string.h>

#include "api.h"
#include "crypto_aead_shared.h"
#include "printstate.h"
#include "shares.h"

#ifdef SS_VER
#include "hal.h"
#else
#define trigger_high()
#define trigger_low()
#endif

/*
 * Preallocated scratchpad for shares to avoid heap usage.
 * Sizes are based on the maximum filesystem body size (8540 bytes).
 * NUM_WORDS(8540) = 2136.
 */
#define MAX_SHARE_WORDS 2136

static mask_m_uint32_t ms_scratch[MAX_SHARE_WORDS];
static mask_c_uint32_t cs_scratch[MAX_SHARE_WORDS + 4];  // Extra space for tag

int crypto_aead_encrypt(unsigned char* c, unsigned long long* clen,
                        const unsigned char* m, unsigned long long mlen,
                        const unsigned char* a, unsigned long long alen,
                        const unsigned char* nsec, const unsigned char* npub,
                        const unsigned char* k) {
  (void)nsec;
  print("encrypt\n");
  printbytes("k", k, CRYPTO_KEYBYTES);
  printbytes("n", npub, CRYPTO_NPUBBYTES);
  printbytes("a", a, alen);
  printbytes("m", m, mlen);

  /* Fixed size buffers for small metadata shares to avoid VLAs */
  mask_key_uint32_t ks[NUM_WORDS(CRYPTO_KEYBYTES)];
  mask_npub_uint32_t ns[NUM_WORDS(CRYPTO_NPUBBYTES)];

  /* AD is unused in filesystem, but we provide a small buffer just in case */
  mask_ad_uint32_t as[NUM_WORDS(16)];

  if (mlen > (MAX_SHARE_WORDS * 4) || alen > 16) return -1;

  /* mask plain input data */
  generate_shares_encrypt(m, ms_scratch, mlen, a, as, alen, npub, ns, k, ks);
  /* call shared interface of ascon encrypt */
  trigger_high();
  crypto_aead_encrypt_shared(cs_scratch, clen, ms_scratch, mlen, as, alen, ns,
                             ks);
  trigger_low();
  /* unmask shared output data */
  combine_shares_encrypt(cs_scratch, c, *clen);

  printbytes("c", c, mlen);
  printbytes("t", c + mlen, CRYPTO_ABYTES);
  print("\n");
  return 0;
}

int crypto_aead_decrypt(unsigned char* m, unsigned long long* mlen,
                        unsigned char* nsec, const unsigned char* c,
                        unsigned long long clen, const unsigned char* a,
                        unsigned long long alen, const unsigned char* npub,
                        const unsigned char* k) {
  int result = 0;
  (void)nsec;
  print("decrypt\n");
  printbytes("k", k, CRYPTO_KEYBYTES);
  printbytes("n", npub, CRYPTO_NPUBBYTES);
  printbytes("a", a, alen);
  printbytes("c", c, clen - CRYPTO_ABYTES);
  printbytes("t", c + clen - CRYPTO_ABYTES, CRYPTO_ABYTES);
  if (clen < CRYPTO_ABYTES) return -1;

  /* Fixed size buffers for small metadata shares to avoid VLAs */
  mask_key_uint32_t ks[NUM_WORDS(CRYPTO_KEYBYTES)];
  mask_npub_uint32_t ns[NUM_WORDS(CRYPTO_NPUBBYTES)];

  /* AD is unused in filesystem, but we provide a small buffer just in case */
  mask_ad_uint32_t as[NUM_WORDS(16)];

  if (clen > (MAX_SHARE_WORDS * 4) || alen > 16) return -1;

  /* mask plain input data */
  generate_shares_decrypt(c, cs_scratch, clen, a, as, alen, npub, ns, k, ks);
  /* call shared interface of ascon decrypt */
  trigger_high();
  result = crypto_aead_decrypt_shared(ms_scratch, mlen, cs_scratch, clen, as,
                                      alen, ns, ks);
  trigger_low();
  /* unmask shared output data */
  combine_shares_decrypt(ms_scratch, m, *mlen);

  printbytes("m", m, *mlen);
  print("\n");
  return result;
}

/*
 * Compatibility wrappers for the legacy ascon_aead API used in filesystem.c
 */

int ascon_aead_encrypt(uint8_t* t, uint8_t* c, const uint8_t* m, uint64_t mlen,
                       const uint8_t* ad, uint64_t adlen, const uint8_t* npub,
                       const uint8_t* k) {
  unsigned long long clen = 0;
  int ret = crypto_aead_encrypt(c, &clen, m, mlen, ad, adlen, NULL, npub, k);
  if (ret == 0) {
    memcpy(t, c + mlen, 16);  // Copy tag to separate buffer
  }
  return ret;
}

int ascon_aead_decrypt(uint8_t* m, const uint8_t* t, const uint8_t* c,
                       uint64_t clen, const uint8_t* ad, uint64_t adlen,
                       const uint8_t* npub, const uint8_t* k) {
  unsigned long long mlen = 0;
  /* NIST API expects tag at the end of c. We use our scratchpad. */
  if (clen > (MAX_SHARE_WORDS * 4)) return -1;

  uint8_t* temp_c = (uint8_t*)ms_scratch;  // use ms_scratch as temp raw buffer
  memcpy(temp_c, c, clen);
  memcpy(temp_c + clen, t, 16);

  return crypto_aead_decrypt(m, &mlen, NULL, temp_c, clen + 16, ad, adlen, npub,
                             k);
}

int ascon_aead_decrypt_stream(uint8_t* m, const uint8_t* c, uint64_t clen,
                              const uint8_t* ad, uint64_t adlen,
                              const uint8_t* npub, const uint8_t* k) {
  unsigned long long mlen = 0;
  mask_key_uint32_t ks[NUM_WORDS(CRYPTO_KEYBYTES)];
  mask_npub_uint32_t ns[NUM_WORDS(CRYPTO_NPUBBYTES)];
  mask_ad_uint32_t as[NUM_WORDS(16)];

  if (clen > (MAX_SHARE_WORDS * 4) || adlen > 16) return -1;

  /* Use ms_scratch as temp for raw input if c and m overlap */
  const uint8_t* src_c = c;
  if (c == m) {
    memcpy(ms_scratch, c, clen);
    src_c = (uint8_t*)ms_scratch;
  }

  /* mask input data */
  generate_shares((uint32_t*)ks, NUM_SHARES_KEY, k, CRYPTO_KEYBYTES);
  generate_shares((uint32_t*)ns, NUM_SHARES_NPUB, npub, CRYPTO_NPUBBYTES);
  generate_shares((uint32_t*)as, NUM_SHARES_AD, ad, adlen);
  generate_shares((uint32_t*)cs_scratch, NUM_SHARES_C, src_c, clen);

  /* call stream interface */
  crypto_aead_decrypt_stream_shared(ms_scratch, &mlen, cs_scratch, clen, as,
                                    adlen, ns, ks);

  /* unmask output */
  combine_shares_decrypt(ms_scratch, m, mlen);
  return 0;
}
