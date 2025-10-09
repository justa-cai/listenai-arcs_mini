/**
 ******************************************************************************
 * @file trace.h
 *
 * @brief Declaration for trace buffer.
 *
 * Copyright (C) ListenAI  2024-2025
 *
 ******************************************************************************
 */
#ifndef _TRACE_H_
#define _TRACE_H_

//#include "rwnx_config.h"
//#include "co_int.h"
//#include "co_bool.h"
#include "stdbool.h"

#if NX_TRACE
#define TRACE_FILE_ID_SIZE  9
#define CO_BIT(pos) (1UL << (pos))
/**
 ******************************************************************************
 * @addtogroup TRACE
 * @details
 *
 * @b Usage: \n
 * You can add:
 * - an unfiltered trace point using @ref TRACE macro for printf like trace and
 * @ref TRACE_BUF to trace a buffer.
 * - a filterable trace point (i.e. that can be disabled/enabled at run-time)
 * using the @ref TRACE_FILT or @ref TRACE_BUF_FILT macros.
 *
 * Unfiltered trace points are always traced.\n
 * A filterable trace point is traced if the bit of the trace point level is
 * set in the current filter for the component. See trace_level_<compo> for
 * trace level specific to a given component.\n
 *
 * You can then use @ref trace_set_filter to change the current filter of a
 * given component. As @ref trace_compo_level is stored in shared_memory Host
 * can also directly modify its content.
 * @{
 ******************************************************************************
 */

/**
 ******************************************************************************
 * @brief Initialize trace buffer.
 *
 * Normal usage would be to initialize trace buffer at boot with:
 * - @p force set to false, so that if fw jumps back to boot code (e.g. by
 *      dereferencing invalid pointer), it will appear in the trace.
 * - @p loop set to true to keep the latest trace.
 *
 * In some specific debug cases, it may be useful to start the trace after a
 * specific event and to not use a circular buffer so that it ensure that trace
 * that happened just after that specific event is kept. In this case set @p
 * force to true and @p loop to false.
 *
 * @param[in] force Force buffer initialization even if trace buffer has
 *                  already been initialized.
 * @param[in] loop  Whether trace buffer must be used circular buffer or not
 ******************************************************************************
 */
void trace_init(bool force, bool loop);

/**
 ******************************************************************************
 * @brief Add a trace point in the buffer
 *
 * This function must NOT be called directly. use @ref TRACE, @ref TRACE_BUF
 * or any of the TRACE_<LVL> macro instead.
 *
 * @param[in] id        Unique trace id
 * @param[in] nb_param  Number of parameters to trace
 * @param[in] param     Table of parameters
 * @param[in] trace_buf boolean indicating if TRACE_BUF macro is used. \n
 *                      if true, param = [buffer size, buffer address(16 LSB),
 *                                        buffer address(16 MSB)]
 ******************************************************************************
 */
void trace(uint32_t id, uint16_t nb_param, uint16_t *param, bool trace_buf);

/**
 ******************************************************************************
 * @brief Modify trace level on a given component
 *
 * @param[in] compo_id  Component id (@ref trace_compo).
 *                      use TRACE_COMPO_MAX to configure all components.
 * @param[in] level     Trace level to activate on the component.
 ******************************************************************************
 */
void trace_set_filter(unsigned int compo_id, unsigned int level);

/**
 ******************************************************************************
 * @brief File ID offset in the 24bits trace ID.
 *
 * TRACE_FILE_ID_SIZE is passed on the compilation line so that its value can
 * easily be shared with the trace dictionary generator.
 * Its value is the same for all files.
 ******************************************************************************
 */
#define TRACE_FILE_ID_OFT (24 - TRACE_FILE_ID_SIZE)

/**
 ******************************************************************************
 * @brief Unique Trace ID for current file/line
 *
 * TRACE_FILE_ID is passed on the compilation line and is different for each file.
 ******************************************************************************
 */
#define TRACE_ID ((TRACE_FILE_ID << TRACE_FILE_ID_OFT) + __LINE__)

/**
 ******************************************************************************
 * @brief Macro used to add a unfiltered trace point in the trace buffer
 *
 * It should be called with a printf like prototype, and will call @ref trace
 * function with a unique trace id (@ref TRACE_ID) and the parameters correctly
 * formatted.
 *
 * Current format string supported by the decoder are:
 * - For character :\n
 * use format \%c and pass directly the character as parameter
 * @verbatim TRACE("first char is %c", string[0]) @endverbatim
 *
 * - For 16bits value :\n
 * Use one of the following format \%d, \%u, \%x, \%X and pass directly the
 * value as parameter.
 * @verbatim TRACE("this is a 16bit value %d", myval) @endverbatim
 * It is also possible to add formatting option
 * @verbatim TRACE("this is a 16bit value 0x%04x", myval) @endverbatim
 *
 * - For 32bits value :\n
 * Use one of the following format \%ld, \%lu, \%lx, \%lX and pass the value
 * using @ref TR_32 macro.
 * @verbatim TRACE("this is a 32bit value 0x%08lx", TR_32(myval)) @endverbatim
 *
 * - For a string :\n
 * Use format \%s and pass string using @ref TR_STR_8 macro.
 * @verbatim TRACE("SSID = %s", TR_STR_8(param->ssid)) @endverbatim
 * @note As implied by the name, string are currently limited to 8 characters.
 *
 * - For a pointer :\n
 * Use format \%p and pass the value using @ref TR_PTR macro.
 * @verbatim TRACE("buffer address is %p", TR_PTR(buffer)) @endverbatim
 * @note \%p is actually just a shortcut for \%08lx
 *
 * - For a MAC address :\n
 * Use format \%pM and pass the value using @ref TR_MAC macro.
 * @verbatim TRACE("vif mac = %pM", TR_MAC(mac_addr)) @endverbatim
 *
 * - For a file name : \n
 * Use format \%F and pass TRACE_FILE_ID as parameter
 * @verbatim TRACE("Error in file %F", TRACE_FILE_ID) @endverbatim
 *
 * - For @ref ke_msg_id_t :\n
 * Use format \%kM and pass the message id directly. Using this format it just
 * like a simple 16bits value, but it also indicates the decoder to translate
 * the message id into its real name (if found inside the dictionary).
 * @verbatim TRACE("msg received is receive [%kM]", msg_id)
   => msg received is [MM_VERSION_REQ] @endverbatim
 *
 * - For @ref ke_task_id_t :\n
 * Use format \%kT and pass the task id directly. Using this format it just
 * like a simple 16bits value, but it also indicates the decoder to translate
 * the task id into its real name (if found inside the dictionary) and index.
 * @verbatim TRACE("send message to task [%kT]", msg_id)
   => send message to task [ME(0)] @endverbatim
 *
 * - For @ref ke_state_t :\n
 * Use format \%kS and pass \b both the task id and the state index. Using this
 * format is like tracing 2 16bits value, but it also indicates the decoder to
 * translate these values into a task name and a state name. (task id is needed
 * because state enum is specific to each task).
 * @verbatim TRACE("set [%kS]", task_id, state)
   => set SCANU(0):SCANU_IDLE @endverbatim
 *
 * - For a timestamp :\n
 * Use format \%t and pass the timestamp value as a 32bits value in microsecond
 * (same format as returned by @ref hal_machw_time).
 * The decoder will format the timestamp value like the timestamp of the trace
 * (i.e. \<s>.\<ms>_\<us>) and will show the delta between the timestamp parameter
 * and the timestamp of the trace.
 * @verbatim TRACE("Timer set at %t", TR_32(timer_date))
   => Timer set at @23.234_789 (+25.35 ms) @endverbatim
 *
 * - For a Frame Control :\n
 * Use format \%fc and pass the value as a 16bits value.
 * The decoder will parse this value as a frame control field (as found in MAC header)
 * @verbatim TRACE("Received frame: %fc", mac_hdr->fctl)
   => Received frame: Association response @endverbatim
 *
 * - For a IPv4 address :\n
 * Use format \%pI4 and pass the value using @ref TR_IP4 macro.
 * @verbatim TRACE("ip = %pI4", TR_IP4(ip4_addr)) @endverbatim
 *
 * - For a IPv6 address :\n
 * Use format \%pI6 or \%pI6c and pass the value using @ref TR_IP6 macro.
 * @verbatim TRACE("ipv6 = %pI6", TR_IP6(ip6_addr)) @endverbatim
 *
 ******************************************************************************
 */
#define TRACE(a, ...) {                                            \
        uint16_t __p[] = { __VA_ARGS__ };                          \
        trace(TRACE_ID, sizeof(__p) / sizeof(__p[0]), __p, false); \
}

/**
 ******************************************************************************
 * @brief Macro used to add an unfiltered trace containing a dump of a buffer in
 * the trace buffer
 *
 * It will call @ref trace function with a unique trace id (@ref TRACE_ID) and
 * the parameters correctly formatted.
 *
 * The format string supported by the decoder is:\n
 *  <b> %pB(F|s)?(number_of_characters)?(word_size)?(representation)? </b>
 *
 * - number_of_characters :\n
 * optional group indicating the number of words to print in one line.\n
 * Default value : 8\n
 * @verbatim TRACE_BUF("my_buf: %pB4", my_buf_len, my_buf);
   => my_buf: 00 10 00 20
              40 FF 00 1A @endverbatim
 *
 * - word_size :\n
 * Optional group indication the size of a word.\n
 * Default value : b\n
 * Possible values: \n
 *                  -# b: byte
 *                  -# h: half-word(16 bits)
 *                  -# w: word(32 bits)\n
 * @verbatim TRACE_BUF("my_buf: %pB4h", my_buf_len, my_buf);
   => my_buf: 0010 0020 40FF 001A@endverbatim

 * - representation :\n
 * Optional group indicating the print format of a word\n
 * Default value: x\n
 * Possible values: \n
 *                  -# d: decimal
 *                  -# x: hexadecimal
 *                  -# c: char\n
 * @verbatim TRACE_BUF("my_buf: %pB4hd", my_buf_len, my_buf);
   => my_buf: 16 32 16639 26@endverbatim
 *
 * - 802.11 Frame: \n
 * If format \%pBF is used (instead of only \%pB), then the decoder will treat the buffer
 * as a 802.11 frame. It will then parse the MAC header and dump the rest of the frame
 * just like any other buffer using number_of_characters/word_size/representation
 * parameters if set.
 * @verbatim TRACE_BUF("received frame: %pBF12bx", my_buf_len, my_buf);
   => received frame: Authentication Dur=60us Seq=1322 frag=0
                      A1=aa:06:cc:dd:ee:fe A2=c4:04:15:3d:41:e8 A3=c4:04:15:3d:41:e8
                      00 00 02 00 00 00 dd 09 00 10 18 02
                      00 00 1c 00 00  @endverbatim
 *
 * - String buffer: \n
 * If the format \%pBs is used (instead of only \%pB), then the decoder will treat the
 * buffer as a string buffer. The decoder will stop buffer processing on first null
 * byte (even if there is more data dumped). number_of_characters can be set to limit
 * the size of the decoder output per line. If the limit is reached the decoder acts as
 * if a '\n' character was present in the dump.
 * If set word_size and representation are ignored.
 * @verbatim TRACE_BUF("command buffer: %pBs", my_buf_len, my_buf);
   => command buffer: This is a string split
                      into 2 lines @endverbatim
 *
 * @verbatim TRACE_BUF("command buffer: %pBs8", my_buf_len, my_buf);
   => command buffer: This is
                      a string
                       split
                      into 2 l
                      ines @endverbatim
 *
 *
 *
 * @param[in] format    string specifying how to interpret the data
 * @param[in] size      size of the buffer in bytes (independent from
 *                      number_of_characters in the conversion specification)
 * @param[in] buf       buffer to dump
 ******************************************************************************
 */
#define TRACE_BUF(format, size, buf) {                                 \
        uint16_t __p[] = {(uint16_t)(size), (uint16_t)((uint32_t)buf), \
                          (uint16_t)(((uint32_t)buf) >> 16)};          \
        uint16_t __size = 1 + (size + 1 + ((uint32_t)buf & 0x1)) / 2;  \
        trace(TRACE_ID, __size, __p, true);                            \
}

/**
 ******************************************************************************
 * @brief Macro to trace a 32bits parameters
 *
 * To be used with \%ld, \%lu, \%lx, \%lX format
 * @param[in] a A 32bit value
 ******************************************************************************
 */
#define TR_32(a) (uint16_t)((uint32_t)(a) >> 16), (uint16_t)((uint32_t)(a))

/**
 ******************************************************************************
 * @brief Macro to trace a 64bits parameters
 *
 * To be used with \%lld, \%llu, \%llx, \%llX format
 * @param[in] a A 64bit value
 ******************************************************************************
 */
#define TR_64(a) TR_32(((uint64_t)(a) >> 32)), TR_32(a)

/**
 ******************************************************************************
 * @brief Macro to trace a pointer
 *
 * To be used with \%p format
 * @param[in] p A 32bit address
 ******************************************************************************
 */
#define TR_PTR(p) TR_32(p)

/**
 ******************************************************************************
 * @brief Macro to trace a MAC address
 *
 * To be used with \%pM format
 * @param[in] m Address on a MAC address
 ******************************************************************************
 */
#define TR_MAC(m) ((uint16_t *)(m))[0], ((uint16_t *)(m))[1], ((uint16_t *)(m))[2]

/**
 ******************************************************************************
 * @brief Macro to trace a IPv4 address
 *
 * To be used with \%pI4 format
 * @param[in] m Address on an IPv4 address (Not the ip address as 32bit value)
 ******************************************************************************
 */
#define TR_IP4(m) TR_32(*((uint32_t *)m))

/**
 ******************************************************************************
 * @brief Macro to trace a IPv6 address
 *
 * To be used with \%pI6, \%ipI6c format
 * @param[in] m Address on an IPv6 address
 ******************************************************************************
 */
#define TR_IP6(m) ((uint16_t *)(m))[0], ((uint16_t *)(m))[1], ((uint16_t *)(m))[2] \
        ((uint16_t *)(m))[3], ((uint16_t *)(m))[4], ((uint16_t *)(m))[5]           \
        ((uint16_t *)(m))[6], ((uint16_t *)(m))[7]

/// internal
#ifdef CFG_RWTL
#define _PTR_ALIGN(p) ((uint16_t *)p)
/**
 ******************************************************************************
 * @brief Macro to trace a string
 *
 * To be used with \%s format. Trace only the 8 first characters of the string
 * @note Using this macro will always store 8bytes in the trace buffer, and
 * decoder will stop as soon as it read 8 characters or '\0' is found.
 ******************************************************************************
 */
#define TR_STR_8(s) (0x0800),                                     \
    (_PTR_ALIGN(s)[0] & 0xFF) + ((_PTR_ALIGN(s)[1] & 0xFF) << 8), \
    (_PTR_ALIGN(s)[2] & 0xFF) + ((_PTR_ALIGN(s)[3] & 0xFF) << 8), \
    (_PTR_ALIGN(s)[4] & 0xFF) + ((_PTR_ALIGN(s)[5] & 0xFF) << 8), \
    (_PTR_ALIGN(s)[6] & 0xFF) + ((_PTR_ALIGN(s)[7] & 0xFF) << 8)


#else
#define _PTR_ALIGN(p) ((uint16_t *)((uint32_t)p & ~0x1))
/**
 ******************************************************************************
 * @brief Macro to trace a string
 *
 * To be used with \%s format. Trace only the 8 first characters of the string
 * @note Using this macro will always store 8bytes in the trace buffer, and
 * decoder will stop as soon as it read 8 characters or '\0' is found.
 ******************************************************************************
 */
#define TR_STR_8(s) (0x0800 + (uint16_t)((uint32_t)(s) & 0x1)), \
    _PTR_ALIGN(s)[0], _PTR_ALIGN(s)[1],                         \
    _PTR_ALIGN(s)[2], _PTR_ALIGN(s)[3]

#endif

/**
 ******************************************************************************
 * @brief Acts like @ref TRACE macro if bit @p lvl is set in trace level for
 * @p compo. Does nothing otherwise.
 * @param[in] compo Name of the component from @ref trace_compo.
 *                  TRACE_COMPO_ prefix should be omitted.
 * @param[in] lvl   Trace level form one of trace_level_... enum.
 *                  TRACE_LVL_ prefix should be omitted
 ******************************************************************************
 */
#define TRACE_FILT(compo, lvl, ...)                                           \
    if (trace_compo_level[TRACE_COMPO_ ## compo] & CO_BIT(TRACE_LVL_ ## lvl)) \
    {                                                                         \
        TRACE(__VA_ARGS__)                                                    \
    }

/**
 ******************************************************************************
 * @brief Acts like @ref TRACE_BUF macro if bit @p lvl is set in trace level
 * for @p compo. Does nothing otherwise.
 * @param[in] compo Name of the component from @ref trace_compo.
 *                  TRACE_COMPO_ prefix should be omitted.
 * @param[in] lvl   Trace level form one of trace_level_... enum.
 *                  TRACE_LVL_ prefix should be omitted
 ******************************************************************************
 */
#define TRACE_BUF_FILT(compo, lvl, ...)                                       \
    if (trace_compo_level[TRACE_COMPO_ ## compo] & CO_BIT(TRACE_LVL_ ## lvl)) \
    {                                                                         \
        TRACE_BUF(__VA_ARGS__)                                                \
    }

/**
 ******************************************************************************
 * @brief Test whether a specific compo/lvl is currently being traced.
 *
 * Can be used to skip several calls TRACE_FILT(compo, lvl, ...).
 *
 * @param[in] compo Name of the component from @ref trace_compo.
 *                  TRACE_COMPO_ prefix should be omitted.
 * @param[in] lvl   Trace level form one of trace_level_... enum.
 *                  TRACE_LVL_ prefix should be omitted
 ******************************************************************************
 */
#define TRACE_FILT_ON(compo, lvl) \
    (trace_compo_level[TRACE_COMPO_ ## compo] & CO_BIT(TRACE_LVL_ ## lvl))

#else /* ! NX_TRACE */

#define TRACE(...)
#define TRACE_BUF(...)
#define TRACE_FILT(...)
#define TRACE_BUF_FILT(...)
#define TRACE_FILT_ON(...) 0
static inline void trace_init(bool force, bool loop) {}

#endif /* NX_TRACE*/

#include "trace_compo.h"

#endif /* _TRACE_H_ */

/// @} end of group
