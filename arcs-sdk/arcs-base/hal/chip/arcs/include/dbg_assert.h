/*
    dbg_assert.h
*/

// #undef assert

// #ifdef NDEBUG           /* required by ANSI standard */
// # define assert(__e) ((void)0)
// #else
// extern void __dbg_assert();
// #ifndef assert
// #define assert(__e) ((__e) ? (void)0 : __dbg_assert())
// #endif

// #endif

// #define ASSERT_ERR assert