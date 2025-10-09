#ifndef FIXED_ARM8_H
#define FIXED_ARM8_H

#undef MULT16_16
static inline spx_word32_t MULT16_16(spx_word16_t a, spx_word16_t b) {
    int result;
    __asm volatile ("smulbb %0, %1, %2" : "=r" (result) : "r" (a), "r" (b));
    return result;
}


#undef MAC16_16
static inline spx_word32_t MAC16_16(spx_word32_t acc, spx_word16_t a, spx_word16_t b) {
  int product;
  __asm volatile ("smulbb %0, %1, %2" : "=r" (product) : "r" (a), "r" (b));
  return acc + product;
}

#undef MULT16_32_P15
static inline spx_word32_t MULT16_32_P15(spx_word16_t a, spx_word32_t b) {
    int result, part1, part2;

    __asm volatile (
        // 计算 part1 = MULT16_32_32(a, b >> 15)
        "asr %1, %3, #15          \n"   // part1 = b >> 15
        "smulwb %1, %2, %1        \n"   // part1 = a * part1 (16x32)

        // 计算 part2 = MULT16_16(a, b & 0x7FFF)
        "and %2, %3, #0x7FFF      \n"   // part2 = b & 0x7FFF
        "smulbb %2, %4, %2        \n"   // part2 = a * part2 (16x16)

        // PSHR(part2, 15): part2 = (part2 + (1 << 14)) >> 15
        "add %2, %2, #0x4000      \n"   // part2 += 1 << 14
        "asr %2, %2, #15          \n"   // part2 >>= 15

        // result = part1 + part2
        "add %0, %1, %2           \n"   // result = part1 + part2

        : "=r" (result), "=r" (part1), "=r" (part2)
        : "r" (b), "r" (a)
        : "cc"
    );

    return result;
}

#undef MAC16_32_Q15
static inline spx_word32_t MAC16_32_Q15(spx_word32_t acc, spx_word16_t a, spx_word32_t b) {
    int product;
    __asm volatile (
        "smulwb %0, %1, %2 \n"  // 16x32 位乘法
        "lsl %0, %0, #1"        // 左移 1 位，相当于 >> 15
        : "=r" (product)
        : "r" (a), "r" (b)
    );
    return acc + product;
}

#undef DIV32_16
static inline short DIV32_16(int a, int b)
{
    int result;
    __asm volatile (
        "sdiv %0, %1, %2"
        : "=r" (result)
        : "r" (a), "r" (b)
    );
    return result;
}

#endif
