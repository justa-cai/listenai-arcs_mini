#ifndef _UTILS_MATH_H_
#define _UTILS_MATH_H_

#include <nmsis_gcc.h>      // for __INLINE

#ifndef CO_ALIGN4_HI
#define CO_ALIGN4_HI(val) (((val) + 3) & ~3)
#endif

 /****************************************************************************************
 * @brief Count leading zeros.
 * @param[in] val Value to count the number of leading zeros on.
 * @return Number of leading zeros when value is written as 32 bits.
 ****************************************************************************************
 */
__STATIC_INLINE uint32_t co_clz(uint32_t val)
{
//    #if defined(__arm__)
//    return __builtin_clz(val);
//    #elif defined(__GNUC__)
//    if (val == 0)
//    {
//        return 32;
//    }
//    return __builtin_clz(val);
//    #else
    uint32_t tmp;
    uint32_t shift = 0;

    if (val == 0)
    {
        return 32;
    }

    tmp = val >> 16;
    if (tmp)
    {
        shift = 16;
        val = tmp;
    }

    tmp = val >> 8;
    if (tmp)
    {
        shift += 8;
        val = tmp;
    }

    tmp = val >> 4;
    if (tmp)
    {
        shift += 4;
        val = tmp;
    }

    tmp = val >> 2;
    if (tmp)
    {
        shift += 2;
        val = tmp;
    }

    tmp = val >> 1;
    if (tmp)
    {
        shift += 1;
    }

    return (31 - shift);
//    #endif // defined(__arm__)
}
#endif // _CO_ENDIAN_H_
