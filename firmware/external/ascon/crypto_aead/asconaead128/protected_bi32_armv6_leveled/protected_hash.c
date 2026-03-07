#include <string.h>

#include "ascon.h"
#include "constants.h"
#include "interleave.h"
#include "permutations.h"
#include "shares.h"

static void ascon_inithash_masked(ascon_state_t* s) {
  int i;
  for (i = 0; i < 6; i++) {
    s->x[i] = MZERO(NUM_SHARES_KEY);
  }

  uint32_t iv_e, iv_o;
  BD(iv_e, iv_o, (uint32_t)ASCON_XOF_IV, (uint32_t)(ASCON_XOF_IV >> 32));
  s->x[0].s[0].w[0] ^= iv_e;
  s->x[0].s[0].w[1] ^= iv_o;

  P(s, 12, NUM_SHARES_KEY);
}

static void ascon_absorb_masked(ascon_state_t* s, const uint8_t* in,
                                uint64_t inlen) {
  uint32_t temp_shares[2 * NUM_SHARES_KEY];
  while (inlen >= 8) {
    generate_shares(temp_shares, NUM_SHARES_KEY, in, 8);
    word_t m = MLOAD(temp_shares, NUM_SHARES_KEY);

    s->x[0] = MXOR(s->x[0], m, NUM_SHARES_KEY);
    P(s, 12, NUM_SHARES_KEY);
    in += 8;
    inlen -= 8;
  }

  // Final partial block + padding
  uint8_t padded[8] = {0};
  memcpy(padded, in, (size_t)inlen);
  padded[inlen] = 0x80;

  generate_shares(temp_shares, NUM_SHARES_KEY, padded, 8);
  word_t m = MLOAD(temp_shares, NUM_SHARES_KEY);
  s->x[0] = MXOR(s->x[0], m, NUM_SHARES_KEY);
}

static void ascon_squeeze_masked(ascon_state_t* s, uint8_t* out,
                                 uint64_t outlen) {
  uint32_t temp_shares[2 * NUM_SHARES_KEY];
  P(s, 12, NUM_SHARES_KEY);
  while (outlen > 8) {
    MSTORE(temp_shares, s->x[0], NUM_SHARES_KEY);
    combine_shares(out, 8, temp_shares, NUM_SHARES_KEY);

    P(s, 12, NUM_SHARES_KEY);
    out += 8;
    outlen -= 8;
  }

  MSTORE(temp_shares, s->x[0], NUM_SHARES_KEY);
  uint8_t last_word[8];
  combine_shares(last_word, 8, temp_shares, NUM_SHARES_KEY);
  memcpy(out, last_word, (size_t)outlen);
}

int ascon_xof_masked(uint8_t* out, uint64_t outlen, const uint8_t* in,
                     uint64_t inlen) {
  ascon_state_t s;
  ascon_inithash_masked(&s);
  ascon_absorb_masked(&s, in, inlen);
  ascon_squeeze_masked(&s, out, outlen);
  return 0;
}
