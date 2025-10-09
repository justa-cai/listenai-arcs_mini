#ifndef __LISTEN_AI_SHELL_HEADER__
#define __LISTEN_AI_SHELL_HEADER__

#include <assert.h>
#include <autoconf.h>

#ifdef BIT
#undef BIT
#endif
#define BIT(NB)                 (1 << (NB))

#ifdef BITS
#undef BITS
#endif
#define BITS(HB, LB)            ((2 << (HB)) - (1 << (LB)))
#define NSTR(R)                 #R
#define N2STR(R)                NSTR(R)
#define REG(ADDR)               (*(volatile unsigned int *)(ADDR))
#define __NAME__                (__builtin_strrchr(__FILE__,'/')?__builtin_strrchr(__FILE__,'/')+1:__FILE__)

#if CONFIG_SHELL_NEWLINE_CRLF
    #define CRLF                "\r\n"
#elif CONFIG_SHELL_NEWLINE_CR
    #define CRLF                "\r"
#elif CONFIG_SHELL_NEWLINE_LF
    #define CRLF                "\n"
#else
    #define CRLF                ""
#endif

#define CLRSCR                  "\e[2J\e[1H"

#if CONFIG_SHELL_COLOR_ENABLE
#define CSI(COR)                "\e[" N2STR(COR) "m"
#else
#define CSI(COR)
#endif

#define CORDEF                  CSI(0)
#define COR_HIGHLIGHT           CSI(1)
#define COR_UNDERLINE           CSI(4)
#define COR_BLINK               CSI(5)
#define COR_REVERSE             CSI(7)
#define COR_BLANK               CSI(8)

#define COR_FG_BLACK            CSI(30)
#define COR_FG_RED              CSI(31)
#define COR_FG_GREEN            CSI(32)
#define COR_FG_YELLOW           CSI(33)
#define COR_FG_BLUE             CSI(34)
#define COR_FG_FUCHSIN          CSI(35)
#define COR_FG_CYAN             CSI(36)
#define COR_FG_WHITE            CSI(37)
#define COR_FG_L_BLACK          CSI(90)
#define COR_FG_L_RED            CSI(91)
#define COR_FG_L_GREEN          CSI(92)
#define COR_FG_L_YELLOW         CSI(93)
#define COR_FG_L_BLUE           CSI(94)
#define COR_FG_L_FUCHSIN        CSI(95)
#define COR_FG_L_CYAN           CSI(96)
#define COR_FG_L_WHITE          CSI(97)

#define COR_BG_BLACK            CSI(40)
#define COR_BG_RED              CSI(41)
#define COR_BG_GREEN            CSI(42)
#define COR_BG_YELLOW           CSI(43)
#define COR_BG_BLUE             CSI(44)
#define COR_BG_FUCHSIN          CSI(45)
#define COR_BG_CYAN             CSI(46)
#define COR_BG_WHITE            CSI(47)
#define COR_BG_L_BLACK          CSI(100)
#define COR_BG_L_RED            CSI(101)
#define COR_BG_L_GREEN          CSI(102)
#define COR_BG_L_YELLOW         CSI(103)
#define COR_BG_L_BLUE           CSI(104)
#define COR_BG_L_FUCHSIN        CSI(105)
#define COR_BG_L_CYAN           CSI(106)
#define COR_BG_L_WHITE          CSI(107)

#define LL_NONE                 (0) // none
#define LL_ERR                  (1) // error
#define LL_WARN                 (2) // warning
#define LL_INFO                 (3) // information
#define LL_DBG                  (4) // debug
#define LL_VERB                 (5) // verbose
#define LL_LEVEL                (CONFIG_SHELL_DEBUG_LEVEL)

extern int printf(const char *__restrict fmt, ...);
extern int printk(const char *__restrict fmt, ...);
extern void flush(void);
extern double systime(void);

#define FLUSH()                 flush()
#define PRINTK(FMT, ...)        printk(FMT, ##__VA_ARGS__)
#define DEBUG(FMT, ...)         printk(FMT CRLF, ##__VA_ARGS__)
#define PRINTF(FMT, ...)        printf(FMT, ##__VA_ARGS__)
#define TRACE(FMT, ...)         printf(FMT CRLF, ##__VA_ARGS__)
#define _LOG(LVL,COR,FMT,...)   do { if (LVL <= LL_LEVEL) PRINTF(COR "(%0.3f) " FMT, systime(), ##__VA_ARGS__); } while (0)
#define LOGE(FMT, ...)          _LOG(LL_ERR,  COR_FG_RED    , FMT CRLF, ##__VA_ARGS__)
#define LOGW(FMT, ...)          _LOG(LL_WARN, COR_FG_FUCHSIN, FMT CRLF, ##__VA_ARGS__)
#define LOGI(FMT, ...)          _LOG(LL_INFO, COR_FG_YELLOW , FMT CRLF, ##__VA_ARGS__)
#define LOGD(FMT, ...)          _LOG(LL_DBG,  COR_FG_BLUE   , FMT CRLF, ##__VA_ARGS__)
#define LOGV(FMT, ...)          _LOG(LL_VERB, COR_FG_CYAN   , FMT CRLF, ##__VA_ARGS__)
#define WARNIF(exp, fmt, ...)   do { if (!(exp)) LOGW("WARN: " fmt, ##__VA_ARGS__); } while (0)
#define ASSERT(exp, fmt, ...)   do { if (!(exp)) { LOGE("ASSERED: " fmt, ##__VA_ARGS__); assert(exp); } } while (0)

typedef struct {
    const char *   scmd;
    const void *   func;
    const char *   desc;
    const uint32_t attr;
} shell_item_t;

#define SHELL_SELF_EXPORT(_func, _desc) SHELL_ITEM_EXPORT(#_func, _func, _desc)
#define SHELL_ITEM_EXPORT(_scmd, _func, _desc) {                                               \
    __attribute__((section(".shell.user"), used, aligned(4)))                                  \
    static const shell_item_t shexp_##_func = {                                                \
        .scmd = (const char *)(_scmd),                                                         \
        .func = (const void *)(_func),                                                         \
        .desc = (const char *)(_desc),                                                         \
    };                                                                                         \
    ASSERT(shexp_##_func.func == _func, "");                                                   \
}

typedef int (*shell_writes_t)(const void *data, int rept, void *const user);

void *shell_init(shell_writes_t writes, void *const base, void *const user);
void  shell_uninit(void);
void  shell_recv_proc(void *shell, char data);
void  shell_print_endl(void *shell, const char *buf, int len);
void  shell_dbg_dump(const void *addr, int size);
void *shell_malloc(int size);
void  shell_free(void *ptr);

#endif //__LISTEN_AI_SHELL_HEADER__
