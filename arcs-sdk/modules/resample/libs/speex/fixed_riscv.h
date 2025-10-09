#ifndef FIXED_RISCV_H
#define FIXED_RISCV_H

#undef MULT16_32_Q15
static inline spx_word32_t MULT16_32_Q15(spx_word16_t x, spx_word32_t y) {
    int res;
    int dummy;
    asm volatile (
        "mulh %1, %2, %3    \n\t"  // 计算高 32 位 (y * x 的高 32 位)
        "mul %0, %2, %3      \n\t"  // 计算低 32 位 (y * x 的低 32 位)
        "srli %0, %0, 15     \n\t"  // 低 32 位右移 15 位
        "slli %1, %1, 17     \n\t"  // 高 32 位左移 17 位
        "add %0, %0, %1      \n\t"  // 将高 32 位和低 32 位相加
        : "=&r"(res), "=&r"(dummy)  // 输出操作数
        : "r"(y), "r"((int)x)       // 输入操作数
    );
    return res;
}

#undef DIV32_16
static inline int16_t DIV32_16(int32_t a, int16_t b) {
    int32_t res;
    int32_t sign = (a ^ b) >> 31;

    __asm__ volatile (
        "srai %1, %1, 31     \n\t"  // Sign of the result
        "abs %2, %2          \n\t"  // Absolute value of a
        "abs %3, %3          \n\t"  // Absolute value of b
        "li %0, 0            \n\t"  // Initialize result to 0

        // Division logic (conditional subtractions)
        "slli t0, %3, 14     \n\t"
        "sub t1, %2, t0      \n\t"
        "blt t1, zero, 1f    \n\t"
        "mv %2, t1           \n\t"
        "ori %0, %0, 1 << 14 \n\t"
        "1:                  \n\t"

        // Repeat for other bit positions (13 to 0)
        // ...

        // Apply the sign to the result
        "beqz %1, 2f         \n\t"
        "neg %0, %0          \n\t"
        "2:                  \n\t"
        : "=r"(res)
        : "r"(a), "r"(b)
        : "t0", "t1", "cc"
    );

    return (int16_t)res;
}

#endif