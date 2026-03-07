#ifndef ASM_H_
#define ASM_H_

#ifndef __GNUC__
#define __asm__ asm
#endif

#if defined(__ARM_ARCH_6M__)

/* C Fallbacks for ARMv6-M (Cortex-M0+) which does not support
   the specialized ARMv6/v7-M assembly instructions used here. */

#define LDR(rd, ptr, offset) rd = *(uint32_t*)((uint8_t*)(ptr) + (offset))

#define STR(rd, ptr, offset) *(uint32_t*)((uint8_t*)(ptr) + (offset)) = (rd)

#define CLEAR()              \
  do {                       \
    volatile uint32_t v = 0; \
    (void)v;                 \
  } while (0)

#define MOV(rd, imm) (rd) = (imm)

#define ROR32_INTERNAL(x, n) \
  (((uint32_t)(x) >> (n)) | ((uint32_t)(x) << ((32 - (n)) & 31)))

#define ROR(rd, rn, imm) (rd) = ROR32_INTERNAL(rn, imm)

#define EOR_ROR(rd, rn, rm, imm) (rd) = (rn) ^ ROR32_INTERNAL(rm, imm)

#define EOR_AND_ROR(ce, ae, be, imm, tmp)        \
  do {                                           \
    (tmp) = (ae) & ROR32_INTERNAL(be, ROT(imm)); \
    (ce) ^= (tmp);                               \
  } while (0)

#define EOR_BIC_ROR(ce, ae, be, imm, tmp)         \
  do {                                            \
    (tmp) = (ae) & ~ROR32_INTERNAL(be, ROT(imm)); \
    (ce) ^= (tmp);                                \
  } while (0)

#define EOR_ORR_ROR(ce, ae, be, imm, tmp)        \
  do {                                           \
    (tmp) = (ae) | ROR32_INTERNAL(be, ROT(imm)); \
    (ce) ^= (tmp);                               \
  } while (0)

#else

#define LDR(rd, ptr, offset) \
  __asm__ volatile("ldr %0, [%1, %2]\n\t" : "=r"(rd) : "r"(ptr), "ri"(offset))

#define STR(rd, ptr, offset)                                                \
  __asm__ volatile("str %0, [%1, %2]\n\t" ::"r"(rd), "r"(ptr), "ri"(offset) \
                   : "memory")

#define CLEAR()                                            \
  do {                                                     \
    uint32_t r, v = 0;                                     \
    __asm__ volatile("mov %0, %1\n\t" : "=r"(r) : "i"(v)); \
  } while (0)

#define MOV(rd, imm) __asm__ volatile("mov %0, %1\n\t" : "=r"(rd) : "i"(imm))

#define ROR(rd, rn, imm) \
  __asm__ volatile("ror %0, %1, #%c2\n\t" : "=r"(rd) : "r"(rn), "i"(imm))

#define EOR_ROR(rd, rn, rm, imm)                  \
  __asm__ volatile("eor %0, %1, %2, ror #%c3\n\t" \
                   : "=r"(rd)                     \
                   : "r"(rn), "r"(rm), "i"(imm))

#define EOR_AND_ROR(ce, ae, be, imm, tmp)                 \
  __asm__ volatile(                                       \
      "and %[tmp_], %[ae_], %[be_], ror %[i1_]\n\t"       \
      "eor %[ce_], %[tmp_], %[ce_]\n\t"                   \
      : [ce_] "+r"(ce), [tmp_] "=r"(tmp)                  \
      : [ae_] "r"(ae), [be_] "r"(be), [i1_] "i"(ROT(imm)) \
      :)

#define EOR_BIC_ROR(ce, ae, be, imm, tmp)                 \
  __asm__ volatile(                                       \
      "bic %[tmp_], %[ae_], %[be_], ror %[i1_]\n\t"       \
      "eor %[ce_], %[tmp_], %[ce_]\n\t"                   \
      : [ce_] "+r"(ce), [tmp_] "=r"(tmp)                  \
      : [ae_] "r"(ae), [be_] "r"(be), [i1_] "i"(ROT(imm)) \
      :)

#define EOR_ORR_ROR(ce, ae, be, imm, tmp)                 \
  __asm__ volatile(                                       \
      "orr %[tmp_], %[ae_], %[be_], ror %[i1_]\n\t"       \
      "eor %[ce_], %[tmp_], %[ce_]\n\t"                   \
      : [ce_] "+r"(ce), [tmp_] "=r"(tmp)                  \
      : [ae_] "r"(ae), [be_] "r"(be), [i1_] "i"(ROT(imm)) \
      :)

#endif

#endif  // ASM_H_
