#ifdef MANUAL_ALLOC

#define TAG "speex"
#include "lisa_log.h"
#include "lisa_mem.h"

/* To avoid changing the Speex call model, this file relies on four static variables
   The user main creates two linear buffers, and initializes spxGlobalHeap/ScratchPtr
   to point to the start of the two buffers, and initializes spxGlobalHeap/ScratchEnd
   to point to the first address following the last byte of the two buffers.

   This mechanism allows, for example, data caching for multichannel applications,
   where the Speex state is swapped from a large slow memory to a small fast memory
   each time the codec runs.

   Persistent data is allocated in spxGlobalHeap (instead of calloc), while scratch
   data is allocated in spxGlobalScratch.
*/

#define OVERRIDE_SPEEX_ALLOC
static inline void *speex_alloc(int size)
{
	return lisa_mem_calloc(1, size);
}

#define OVERRIDE_SPEEX_ALLOC_SCRATCH
static inline void *speex_alloc_scratch(int size)
{
	return lisa_mem_calloc(1, size);
}

#define OVERRIDE_SPEEX_REALLOC
static inline void *speex_realloc(void *ptr, int size)
{
	return lisa_mem_realloc(ptr, size);
}

#define OVERRIDE_SPEEX_FREE
static inline void speex_free(void *ptr)
{
	lisa_mem_free(ptr);
}

#define OVERRIDE_SPEEX_FREE_SCRATCH
static inline void speex_free_scratch(void *ptr)
{
	lisa_mem_free(ptr);
}

#define OVERRIDE_SPEEX_FATAL
static inline void _speex_fatal(const char *str, const char *file, int line)
{
	LISA_NLOGE("Fatal (internal) error in %s, line %d: %s", file, line, str);
}

#define OVERRIDE_SPEEX_WARNING
static inline void speex_warning(const char *str)
{
#ifndef DISABLE_WARNINGS
	LISA_NLOGW("warning: %s", str);
#endif
}

#define OVERRIDE_SPEEX_WARNING_INT
static inline void speex_warning_int(const char *str, int val)
{
#ifndef DISABLE_WARNINGS
	LISA_NLOGW("warning: %s %d\n", str, val);
#endif
}

#define OVERRIDE_SPEEX_NOTIFY
static inline void speex_notify(const char *str)
{
#ifndef DISABLE_NOTIFICATIONS
	LISA_NLOGI("notification: %s\n", str);
#endif
}

#define OVERRIDE_SPEEX_PUTC
static inline void _speex_putc(int ch, void *file)
{
}

#endif /* !MANUAL_ALLOC */