#include "crypto_aead.h"


#include "api.h"
#include "ascon.h"
#include "printstate.h"
#include "shares.h"

#ifdef SS_VER
#include "hal.h"
#else
#define trigger_high()
#define trigger_low()
#endif

/*
 * Chunked scratchpad for shares.
 * Instead of preallocating buffers for the entire message (~34KB with
 * NUM_SHARES=2), we process the message in CHUNK_SIZE-byte pieces.
 * CHUNK_SIZE must be a multiple of ASCON_AEAD_RATE (16).
 *
 * With NUM_SHARES up to 4 and CHUNK_SIZE=256:
 *   ms_chunk: NUM_WORDS(256) * sizeof(mask_m_uint32_t) = 64 * 4*4 = 1024 bytes max
 *   cs_chunk: (NUM_WORDS(256)+4) * sizeof(mask_c_uint32_t) = 68 * 4*4 = 1088 bytes max
 * Total: ~2KB instead of ~34KB.
 */
#define CHUNK_SIZE 256
#define CHUNK_SHARE_WORDS NUM_WORDS(CHUNK_SIZE)

/* Max AD size: handler passes filename(32) + sizeof(FS_FileEntry)(24) = 56 bytes */
#define MAX_AD_SIZE 64

static mask_m_uint32_t ms_chunk[CHUNK_SHARE_WORDS];
static mask_c_uint32_t cs_chunk[CHUNK_SHARE_WORDS + 4]; /* +4 for tag space */



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

  if (alen > MAX_AD_SIZE) return -1;

  /* Small metadata shares on the stack */
  mask_key_uint32_t ks[NUM_WORDS(CRYPTO_KEYBYTES)];
  mask_npub_uint32_t ns[NUM_WORDS(CRYPTO_NPUBBYTES)];
  mask_ad_uint32_t as[NUM_WORDS(MAX_AD_SIZE)];

  /* Generate shares for key, nonce, AD */
  generate_shares((uint32_t*)ks, NUM_SHARES_KEY, k, CRYPTO_KEYBYTES);
  generate_shares((uint32_t*)ns, NUM_SHARES_NPUB, npub, CRYPTO_NPUBBYTES);
  generate_shares((uint32_t*)as, NUM_SHARES_AD, a, alen);

  /* Initialize sponge state */
  ascon_state_t s;
  ascon_initaead(&s, ks, ns);
#if NUM_SHARES_KEY != NUM_SHARES_AD
  ascon_level_adata(&s);
#endif
  ascon_adata(&s, as, alen);
#if NUM_SHARES_AD != NUM_SHARES_M
  ascon_level_encdec(&s);
#endif

  trigger_high();

  /* Process message in chunks */
  uint64_t offset = 0;

  /* Full chunks: use ascon_encrypt_update (no padding) */
  while (offset + CHUNK_SIZE <= mlen) {
    generate_shares((uint32_t*)ms_chunk, NUM_SHARES_M, m + offset, CHUNK_SIZE);
    ascon_encrypt_update(&s, cs_chunk, ms_chunk, CHUNK_SIZE);
    combine_shares((uint8_t*)(c + offset), CHUNK_SIZE, (uint32_t*)cs_chunk,
                   NUM_SHARES_C);
    offset += CHUNK_SIZE;
  }

  /* Final chunk (may be partial or zero-length): uses ascon_encrypt with
   * padding */
  {
    uint64_t remaining = mlen - offset;
    if (remaining > 0) {
      generate_shares((uint32_t*)ms_chunk, NUM_SHARES_M, m + offset, remaining);
    }
    ascon_encrypt(&s, cs_chunk, ms_chunk, remaining);
    if (remaining > 0) {
      combine_shares((uint8_t*)(c + offset), remaining, (uint32_t*)cs_chunk,
                     NUM_SHARES_C);
    }
  }

  trigger_low();

  /* Finalize and generate tag */
#if NUM_SHARES_M != NUM_SHARES_KEY
  ascon_level_final(&s);
#endif
  ascon_final(&s, ks);
  ascon_settag(&s, cs_chunk);
  combine_shares(c + mlen, CRYPTO_ABYTES, (uint32_t*)cs_chunk, NUM_SHARES_C);

  *clen = mlen + CRYPTO_ABYTES;

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
  (void)nsec;
  print("decrypt\n");
  printbytes("k", k, CRYPTO_KEYBYTES);
  printbytes("n", npub, CRYPTO_NPUBBYTES);
  printbytes("a", a, alen);
  if (clen < CRYPTO_ABYTES) return -1;
  *mlen = clen - CRYPTO_ABYTES;
  printbytes("c", c, *mlen);
  printbytes("t", c + *mlen, CRYPTO_ABYTES);

  if (alen > MAX_AD_SIZE) return -1;

  /* Small metadata shares on the stack */
  mask_key_uint32_t ks[NUM_WORDS(CRYPTO_KEYBYTES)];
  mask_npub_uint32_t ns[NUM_WORDS(CRYPTO_NPUBBYTES)];
  mask_ad_uint32_t as[NUM_WORDS(MAX_AD_SIZE)];

  /* Generate shares for key, nonce, AD */
  generate_shares((uint32_t*)ks, NUM_SHARES_KEY, k, CRYPTO_KEYBYTES);
  generate_shares((uint32_t*)ns, NUM_SHARES_NPUB, npub, CRYPTO_NPUBBYTES);
  generate_shares((uint32_t*)as, NUM_SHARES_AD, a, alen);

  /* Initialize sponge state */
  ascon_state_t s;
  ascon_initaead(&s, ks, ns);
#if NUM_SHARES_KEY != NUM_SHARES_AD
  ascon_level_adata(&s);
#endif
  ascon_adata(&s, as, alen);
#if NUM_SHARES_AD != NUM_SHARES_M
  ascon_level_encdec(&s);
#endif

  trigger_high();

  /* Process ciphertext in chunks */
  uint64_t data_len = *mlen;
  uint64_t offset = 0;

  /* Full chunks: use ascon_decrypt_update (no padding) */
  while (offset + CHUNK_SIZE <= data_len) {
    generate_shares((uint32_t*)cs_chunk, NUM_SHARES_C, c + offset, CHUNK_SIZE);
    ascon_decrypt_update(&s, ms_chunk, cs_chunk, CHUNK_SIZE);
    combine_shares(m + offset, CHUNK_SIZE, (uint32_t*)ms_chunk, NUM_SHARES_M);
    offset += CHUNK_SIZE;
  }

  /* Final chunk (may be partial or zero-length): uses ascon_decrypt with
   * padding */
  {
    uint64_t remaining = data_len - offset;
    if (remaining > 0) {
      generate_shares((uint32_t*)cs_chunk, NUM_SHARES_C, c + offset, remaining);
    }
    ascon_decrypt(&s, ms_chunk, cs_chunk, remaining);
    if (remaining > 0) {
      combine_shares(m + offset, remaining, (uint32_t*)ms_chunk, NUM_SHARES_M);
    }
  }

  trigger_low();

  /* Finalize and verify tag */
#if NUM_SHARES_M != NUM_SHARES_KEY
  ascon_level_final(&s);
#endif
  ascon_final(&s, ks);
  generate_shares((uint32_t*)cs_chunk, NUM_SHARES_C, c + data_len,
                  CRYPTO_ABYTES);
  ascon_xortag(&s, cs_chunk);

  printbytes("m", m, *mlen);
  print("\n");
  return ascon_iszero(&s);
}

/*
 * Compatibility wrappers for the legacy ascon_aead API used in handler.c
 */

int ascon_aead_encrypt(uint8_t* t, uint8_t* c, const uint8_t* m, uint64_t mlen,
                       const uint8_t* ad, uint64_t adlen, const uint8_t* npub,
                       const uint8_t* k) {
  /*
   * Drive the sponge directly with chunked encryption.
   *   t = tag output (CRYPTO_ABYTES = 16 bytes)
   *   c = ciphertext output (mlen bytes, may alias m for in-place)
   *   m = plaintext input (mlen bytes)
   */

  if (adlen > MAX_AD_SIZE) return -1;

  /* Small metadata shares on the stack */
  mask_key_uint32_t ks[NUM_WORDS(CRYPTO_KEYBYTES)];
  mask_npub_uint32_t ns[NUM_WORDS(CRYPTO_NPUBBYTES)];
  mask_ad_uint32_t as_shares[NUM_WORDS(MAX_AD_SIZE)];

  generate_shares((uint32_t*)ks, NUM_SHARES_KEY, k, CRYPTO_KEYBYTES);
  generate_shares((uint32_t*)ns, NUM_SHARES_NPUB, npub, CRYPTO_NPUBBYTES);
  generate_shares((uint32_t*)as_shares, NUM_SHARES_AD, ad, adlen);

  ascon_state_t s;
  ascon_initaead(&s, ks, ns);
#if NUM_SHARES_KEY != NUM_SHARES_AD
  ascon_level_adata(&s);
#endif
  ascon_adata(&s, as_shares, adlen);
#if NUM_SHARES_AD != NUM_SHARES_M
  ascon_level_encdec(&s);
#endif

  /* Chunked encryption: ciphertext written to c */
  uint64_t offset = 0;
  while (offset + CHUNK_SIZE <= mlen) {
    generate_shares((uint32_t*)ms_chunk, NUM_SHARES_M, m + offset, CHUNK_SIZE);
    ascon_encrypt_update(&s, cs_chunk, ms_chunk, CHUNK_SIZE);
    combine_shares(c + offset, CHUNK_SIZE, (uint32_t*)cs_chunk, NUM_SHARES_C);
    offset += CHUNK_SIZE;
  }
  {
    uint64_t remaining = mlen - offset;
    if (remaining > 0) {
      generate_shares((uint32_t*)ms_chunk, NUM_SHARES_M, m + offset, remaining);
    }
    ascon_encrypt(&s, cs_chunk, ms_chunk, remaining);
    if (remaining > 0) {
      combine_shares(c + offset, remaining, (uint32_t*)cs_chunk, NUM_SHARES_C);
    }
  }

  /* Finalize and write tag to t */
#if NUM_SHARES_M != NUM_SHARES_KEY
  ascon_level_final(&s);
#endif
  ascon_final(&s, ks);
  ascon_settag(&s, cs_chunk);
  combine_shares(t, CRYPTO_ABYTES, (uint32_t*)cs_chunk, NUM_SHARES_C);

  return 0;
}

int ascon_aead_decrypt(uint8_t* m, const uint8_t* t, const uint8_t* c,
                       uint64_t clen, const uint8_t* ad, uint64_t adlen,
                       const uint8_t* npub, const uint8_t* k) {
  /*
   * Drive the sponge directly with chunked decryption.
   * The tag (t) is handled separately from the ciphertext (c),
   * so no concatenation buffer is needed.
   */

  if (adlen > MAX_AD_SIZE) return -1;

  /* Small metadata shares on the stack */
  mask_key_uint32_t ks[NUM_WORDS(CRYPTO_KEYBYTES)];
  mask_npub_uint32_t ns[NUM_WORDS(CRYPTO_NPUBBYTES)];
  mask_ad_uint32_t as_shares[NUM_WORDS(MAX_AD_SIZE)];

  generate_shares((uint32_t*)ks, NUM_SHARES_KEY, k, CRYPTO_KEYBYTES);
  generate_shares((uint32_t*)ns, NUM_SHARES_NPUB, npub, CRYPTO_NPUBBYTES);
  generate_shares((uint32_t*)as_shares, NUM_SHARES_AD, ad, adlen);

  ascon_state_t s;
  ascon_initaead(&s, ks, ns);
#if NUM_SHARES_KEY != NUM_SHARES_AD
  ascon_level_adata(&s);
#endif
  ascon_adata(&s, as_shares, adlen);
#if NUM_SHARES_AD != NUM_SHARES_M
  ascon_level_encdec(&s);
#endif

  /* Chunked decryption of ciphertext */
  uint64_t offset = 0;
  while (offset + CHUNK_SIZE <= clen) {
    generate_shares((uint32_t*)cs_chunk, NUM_SHARES_C, c + offset, CHUNK_SIZE);
    ascon_decrypt_update(&s, ms_chunk, cs_chunk, CHUNK_SIZE);
    combine_shares(m + offset, CHUNK_SIZE, (uint32_t*)ms_chunk, NUM_SHARES_M);
    offset += CHUNK_SIZE;
  }
  {
    uint64_t remaining = clen - offset;
    if (remaining > 0) {
      generate_shares((uint32_t*)cs_chunk, NUM_SHARES_C, c + offset, remaining);
    }
    ascon_decrypt(&s, ms_chunk, cs_chunk, remaining);
    if (remaining > 0) {
      combine_shares(m + offset, remaining, (uint32_t*)ms_chunk, NUM_SHARES_M);
    }
  }

  /* Finalize and verify tag (t is separate, not contiguous with c) */
#if NUM_SHARES_M != NUM_SHARES_KEY
  ascon_level_final(&s);
#endif
  ascon_final(&s, ks);
  generate_shares((uint32_t*)cs_chunk, NUM_SHARES_C, t, CRYPTO_ABYTES);
  ascon_xortag(&s, cs_chunk);

  return ascon_iszero(&s);
}
