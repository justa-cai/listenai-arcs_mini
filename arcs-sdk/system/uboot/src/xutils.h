#ifndef __XFER_UTILS_HEADER__
#define __XFER_UTILS_HEADER__

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>

#include "FreeRTOS.h"
#include "semphr.h"
#include "queue.h"
#include "task.h"
#include "event_groups.h"
#include "stream_buffer.h"

#include "arcs_ap.h"

#define __packed__          __attribute__((packed))
#define __naked__           __attribute__((naked))
#define __depre__           __attribute__((deprecated))
#define __setup__           __attribute__((constructor))
#define __noret__           __attribute__((noreturn))
#define __noinline__        __attribute__((noinline))
#define __const__           __attribute__((const))
#define __used__            __attribute__((used))
#define __unused__          __attribute__((unused))
#define __weak__            __attribute__((weak))
#define __align(N)          __attribute__((aligned(N)))
#define __at(addr)          __attribute__((at(addr)))
#define __format(fp,vp)     __attribute__((format(printf, fp, vp)))
#define __hide__            __attribute__((visibility("hidden")))
#define __fast__            __attribute__((section(".fast_text")))
#define __strs__            __attribute__((section(".strdat")))
#define __stub__            __attribute__((section(".stub"), used))
#define __sect(sct)         __attribute__((section(sct)))
#define __optimize(OP)      __attribute__((optimize(OP)))
#define __wrap(F)           __wrap_##F
#define __real(F)           __real_##F
#define static_const        static const
#define static_inline       static inline
#define static_noinline     static __noinline__
#define __maxof(A,B)        ({typeof(A) _a=(A),_b=(B);_a>_b?_a:_b;})
#define __minof(A,B)        ({typeof(A) _a=(A),_b=(B);_a<_b?_a:_b;})
#define __cntof(A)          (sizeof(A)/sizeof(A[0]))
#define __offsof(T,M)       ((size_t)&((T*)0)->M)
#define __contof(P,T,M)     ((T*)(((char*)(P))-__offsof(T,M)))
#define XINFINITE           (portMAX_DELAY)
#define XMS2TK(MS)			((MS)==XINFINITE?portMAX_DELAY:((x_msec_t)(MS)/portTICK_PERIOD_MS))
#define XTK2MS(TK)          ((int)(TK)*portTICK_PERIOD_MS)
#define XEGINT()            __RV_CSR_SET(CSR_MSTATUS,MSTATUS_MIE)
#define XDGINT()            __RV_CSR_CLEAR(CSR_MSTATUS,MSTATUS_MIE)
#define XNOP()              __NOP()
#define XWFI()              __WFI()
#define XWFE()              __WFE()
#define XECALL()            __ECALL()
#define XBREAK()            __EBREAK()
#define XISB()              __FENCE_I()
#define XDMB()              __RWMB()
#define XBIT(NB)			(1<<(NB))
#define XBITS(HB, LB)	    ((2<<(HB))-(1<<(LB)))
#define XCONCAT(A,B)        A##B
#define XVANAME(A,B)        XCONCAT(A,B)
#define __XARGS(_X)         (_X)
#define __XARGN(_0,_1,_2,_3,_4,_5,_6,_7,N,...) (N==1?!!#_0[0]:N)
#define __XARGC(...)        __XARGS(__XARGN(__VA_ARGS__,8,7,6,5,4,3,2,1))
#define XARGC(...)          __XARGC(__VA_ARGS__)
#define NSTR(R)             #R
#define N2STR(R)            NSTR(R)
#define REG(ADDR)           (*(volatile unsigned int *)(ADDR))
#define __NAME__            (__builtin_strrchr(__FILE__,'/')?__builtin_strrchr(__FILE__,'/')+1:__FILE__)

#define OS_PRIO_HIG         (configMAX_PRIORITIES - 1)
#define OS_PRIO_MID         (configMAX_PRIORITIES / 2)
#define OS_PRIO_LOW         (1)
#define OS_PRIO_DEF         (OS_PRIO_MID)
#define OS_STACK_DEF        (configMINIMAL_STACK_SIZE)

#define CRLF                "\n"
#define CLRSCR              "\e[2J\e[1H"
#define CSI(COR)            "\e[" N2STR(COR) "m"

#define CORDEF              CSI(0)
#define COR_HIGHLIGHT       CSI(1)
#define COR_UNDERLINE       CSI(4)
#define COR_BLINK           CSI(5)
#define COR_REVERSE         CSI(7)
#define COR_BLANK           CSI(8)

#define COR_FG_BLACK        CSI(30)
#define COR_FG_RED          CSI(31)
#define COR_FG_GREEN        CSI(32)
#define COR_FG_YELLOW       CSI(33)
#define COR_FG_BLUE         CSI(34)
#define COR_FG_FUCHSIN      CSI(35)
#define COR_FG_CYAN         CSI(36)
#define COR_FG_WHITE        CSI(37)
#define COR_FG_L_BLACK      CSI(90)
#define COR_FG_L_RED        CSI(91)
#define COR_FG_L_GREEN      CSI(92)
#define COR_FG_L_YELLOW     CSI(93)
#define COR_FG_L_BLUE       CSI(94)
#define COR_FG_L_FUCHSIN    CSI(95)
#define COR_FG_L_CYAN       CSI(96)
#define COR_FG_L_WHITE      CSI(97)

#define COR_BG_BLACK        CSI(40)
#define COR_BG_RED          CSI(41)
#define COR_BG_GREEN        CSI(42)
#define COR_BG_YELLOW       CSI(43)
#define COR_BG_BLUE         CSI(44)
#define COR_BG_FUCHSIN      CSI(45)
#define COR_BG_CYAN         CSI(46)
#define COR_BG_WHITE        CSI(47)
#define COR_BG_L_BLACK      CSI(100)
#define COR_BG_L_RED        CSI(101)
#define COR_BG_L_GREEN      CSI(102)
#define COR_BG_L_YELLOW     CSI(103)
#define COR_BG_L_BLUE       CSI(104)
#define COR_BG_L_FUCHSIN    CSI(105)
#define COR_BG_L_CYAN       CSI(106)
#define COR_BG_L_WHITE      CSI(107)

#define LL_NONE             (0) // none
#define LL_ERR              (1) // error
#define LL_WARN             (2) // warning
#define LL_INFO             (3) // information
#define LL_DBG              (4) // debug
#define LL_VERB             (5) // verbose
#define LL_LEVEL            (LL_VERB)

#define PRINTF(FMT, ...)      printf(FMT, ##__VA_ARGS__)
#define TRACE(FMT, ...)       printf(FMT CRLF, ##__VA_ARGS__)
#define _LOG(LVL,COR,FMT,...) do { if (LVL <= LL_LEVEL) PRINTF(COR FMT, ##__VA_ARGS__); } while (0)
#define LOGE(FMT, ...)        _LOG(LL_ERR,  COR_FG_RED    , FMT CRLF, ##__VA_ARGS__)
#define LOGW(FMT, ...)        _LOG(LL_WARN, COR_FG_FUCHSIN, FMT CRLF, ##__VA_ARGS__)
#define LOGI(FMT, ...)        _LOG(LL_INFO, COR_FG_YELLOW , FMT CRLF, ##__VA_ARGS__)
#define LOGD(FMT, ...)        _LOG(LL_DBG,  COR_FG_BLUE   , FMT CRLF, ##__VA_ARGS__)
#define LOGV(FMT, ...)        _LOG(LL_VERB, COR_FG_CYAN   , FMT CRLF, ##__VA_ARGS__)
#define ASSERT(exp, fmt, ...) do { if (!(exp)) { LOGE("ASSERED: " fmt, ##__VA_ARGS__); assert(exp); } } while (0)

typedef void *x_handle_t;
////////////////////////////////////////////////////////////////////////////////////////////////////
// memory apis
void *inram_malloc(size_t align, size_t size);
void *inram_realloc(void *ptr, size_t size);
void *inram_calloc(size_t align, size_t num, size_t size);
void inram_free(void *ptr);
void *exram_malloc(size_t align, size_t size);
void *exram_realloc(void *ptr, size_t size);
void *exram_calloc(size_t align, size_t num, size_t size);
void exram_free(void *ptr);

#endif//__XFER_UTILS_HEADER__
