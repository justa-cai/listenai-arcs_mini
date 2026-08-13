/*!
 * @file dbg_assert.h
 * @brief Custom debugging assertion mechanism implementation.
 *        Provides configurable runtime validation with optional error handling.
 *        This header redefines the standard `assert()` macro to either:
 *          - Do nothing when compiled with NDEBUG (release mode)
 *          - Call internal __dbg_assert() function on failure in debug builds
 */

#undef assert

/**
 * @cond NDEBUG
 * ANSI standard requirement - disable assertions when NDEBUG is defined
 * (typically used for release build optimizations)
 */
#ifdef NDEBUG           /* required by ANSI standard */
# define assert(__e) ((void)0)
/** @endcond */

#else
extern void __dbg_assert();

/**
 * @brief Application-specific assertion macro
 *
 * Evaluates expression (@p __e). If false (0), calls __dbg_assert().
 * Otherwise performs no operation.
 *
 * @param __e Boolean expression to evaluate
 * @note Must be used only in non-NDEBUG compilation modes
 * @note User must provide implementation of __dbg_assert() elsewhere
 *       See also: <tt>__dbg_assert()</tt> function declaration above
 */
# define assert(__e) ((__e) ? (void)0 : __dbg_assert())
#endif

/**
 * @brief AssertMacros Assertion Macros
 * @{
 */
#define ASSERT_ERR assert /**< Alias for standard assert macro */
