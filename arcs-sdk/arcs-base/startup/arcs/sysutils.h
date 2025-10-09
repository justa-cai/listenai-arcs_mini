#ifndef __SYSUTILS_H__
#define __SYSUTILS_H__

#define __weak__ __attribute__((weak))

#define __cmn_sram_bss__  __attribute__((section(".cmn_sram.bss")))

#define __itcm_text__ __attribute__((section(".itcm.text")))

#define __dtcm_data__ __attribute__((section(".dtcm.data")))
#define __dtcm_bss__ __attribute__((section(".dtcm.bss")))

#define __psram_data__ __attribute__((section(".psram.data")))
#define __psram_bss__  __attribute__((section(".psram.bss")))
#define __psram_code__ __attribute__((section(".psram.text")))
#define __psram_noinit__ __attribute__((section(".psram.noinit")))

#define __noinit__ __attribute__((section(".noinit")))

#define __alias__(fn) __attribute__((alias(#fn)))

#endif
