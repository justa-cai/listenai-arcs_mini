#include <stdint.h>
#include <string.h>
#include "core_spinlock.h"

void spinlock_init(volatile uint32_t *lock)
{
    memset((void *)lock, 0, sizeof(*lock));
}

int spinlock_acquire(volatile uint32_t *lock)
{
    if (!lock) {
        return -1;
    }

    uint32_t tmp;
    asm volatile (
        "li %0, 1\n"                // tmp = 1（尝试获取锁）
        "1:\n"
        "amoswap.w.aq %0, %0, (%1)\n" // 原子交换：将 tmp 写入 lock，返回旧值到 tmp
        "bnez %0, 1b\n"             // 如果旧值非零（锁已被占用），循环等待
        : "=&r"(tmp)
        : "r"(lock)
        : "memory"
    );

    return 0;
}

int spinlock_release(volatile uint32_t *lock)
{
    if (!lock) {
        return -1;
    }

    asm volatile (
        "fence rw, rw\n"            // 确保内存操作顺序
        "amoswap.w.rl zero, zero, (%0)\n" // 原子写 0 到 lock
        :
        : "r"(lock)
        : "memory"
    );

    return 0;
}