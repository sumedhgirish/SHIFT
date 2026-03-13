#ifndef __COMMON_H__
#define __COMMON_H__

#include <stdint.h>

#define SECPASS 0x5A5AA5A5U
#define SECFAIL 0xA5A55A5AU

#define IF(cond)                                                               \
    _Pragma("clang optimize off")                                              \
    {                                                                          \
        uint32_t _cond = (cond);                                               \
        uint32_t _check1 = SECFAIL + _cond * (SECPASS - SECFAIL);              \
        uint32_t _check2 = SECFAIL + _cond * (SECPASS - SECFAIL);              \
        uint32_t _check3 = SECFAIL + _cond * (SECPASS - SECFAIL);              \
        if (_check1 == SECPASS && _check2 == SECPASS && _check3 == SECPASS)

#define ENDIF                                                                  \
    }                                                                          \
    _Pragma("clang optimize on")

#endif
