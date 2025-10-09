
#ifndef __SOC_ATTR_H__
#define __SOC_ATTR_H__

// Forces code into IRAM instead of flash
#define IRAM_ATTR //__attribute__((section(".heap.code")))

// Forces a function to be inlined
#define FORCE_INLINE_ATTR static inline __attribute__((always_inline))

// Forces to not inline function
#define NOINLINE_ATTR __attribute__((noinline))

#endif /* __SOC_ATTR_H__ */
