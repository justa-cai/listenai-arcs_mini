/*
 * patch.h
 *
 *  Created on:
 *      Author: Tiger
 */

#ifndef INCLUDE_PATCH_H_
#define INCLUDE_PATCH_H_
#include <stddef.h>
#include "patch_args.h"


#define _PTCH_UNIQUE(prefix) PW_CONCAT(prefix, __LINE__, _, __COUNTER__)
#define _PTCH_FUNCS_SECTION  PW_KEEP_IN_SECTION(PW_STRINGIFY(_PTCH_UNIQUE(.patch.entries.)))

////////////////////////////////////////////////////////////////////////////////////////////
//   use in origin code
//   example:
//   #include "patch.h"
//
//   PTCH_DFN(void, test_isr, int, flag);        // for fast patch, patch define
//   void test_isr(int flag)
//   {
//       PTCH_FST(void, test_isr, int, flag);    // for fast patch, patch it if patch exists
//       ...
//   }
//
//   int test_func(long param1, void *param2)
//   {
//       PATCH(int, test_func, long, param1, void *, param2); // patch it, if patch exists
//       ...
//       return 0;
//   }
//
//   int main()
//   {
//       ...
//       patch_func_ptr = patch_init(NULL);                   // initial patch module
//       ...
//       test_func(10, NULL);
//       ...
//       return 0;
//   }
////////////////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////////////////
//   use in patch code
//   example:
//   #include "patch.h"
//   #include "patch_def.h"                     // generate from symbol table of origin code
//
//	 int test_func2(long param1, void *param2)
//	 {
//       ...                                    // new implement here
//	 }
//
//   void * patch_func_ptr2(void *org_func_ptr)
//   {
//			void *ret = NULL;
//
//			switch((uint32_t)org_func_ptr) {
//			case PTR_xxxxx:                      // the value define in patch_def.h
//				break;
//			case PTR_test_func:                  // the value define in patch_def.h
//             ret = test_func2;                 // return new function pointer
//				break;
//			default:
//				ret = NULL;
//				break;
//			}
//
//			return ret;
//   }
//
//   void test_isr2(int flag)
//   {
//       ...                                    // for fast patch, new implement here
//   }
//
//   void* patch_init2(void *param)
//   {
//       ...                                              // initial data and bss in patch;
//       PTCH_BND(void, test_isr, int, flag) = test_isr2; // for fast patch, bind the variable
//       return patch_func_ptr2;
//   }
//
//   __attribute__((section(".FUNC_TAB"))) const uint32_t patch_hdr[4] = {
//		0x5450U + (0x0100U << 16),
//		(uint32_t)patch_init2,
//		0,   /* image size */
//		0    /* check sum  */
//   };
////////////////////////////////////////////////////////////////////////////////////////////

typedef void* (*patch_func_ptr_t)(void *org_func_ptr);
typedef void* (*patch_init_t)(void *param);

typedef struct
{
    /// magic number, should be 0x5450
    uint16_t flag;
    /// version
    uint16_t version;
    /// patch init function
    patch_init_t fun;
    /// file length
    uint32_t length;
    /// check sum
    uint16_t checksum;
    /// reserved
    uint16_t res;

} patch_header_t  ;

extern patch_func_ptr_t patch_func_ptr;
extern void* patch_init(void *param);
#if defined(BUILD_ROM) && (BUILD_ROM)
#define PTCH(type, name, ...)                  \
        typedef type (*name##_t)(PW_DELEGATE_BY_ARG_COUNT(_PTCH_FUNC_TYPE_ARGS_, __VA_ARGS__));      \
        static char func_name[] _PTCH_FUNCS_SECTION = PW_STRINGIFY(name);                            \
        name##_t name##_pt = patch_func_ptr(name);                                 \
        if(name##_pt) return name##_pt(PW_DELEGATE_BY_ARG_COUNT(_PTCH_FUNC_CALL_ARGS_, __VA_ARGS__))

#else
#define PTCH(type, name, ...)                  \
    do{}while(0)

#endif


#define PTCH_NORETURN(type, name, ...)         \
	typedef type (*name##_t)(PW_DELEGATE_BY_ARG_COUNT(_PTCH_FUNC_TYPE_ARGS_, __VA_ARGS__));      \
	static char func_name[] _PTCH_FUNCS_SECTION = PW_STRINGIFY(name);                            \
	name##_t name##_pt = patch_func_ptr(name);                                 \
	if(name##_pt) name##_pt(PW_DELEGATE_BY_ARG_COUNT(_PTCH_FUNC_CALL_ARGS_, __VA_ARGS__))


// for fast patch. It will use 4 bytes global variable per patch point
#if defined(BUILD_ROM) && (BUILD_ROM)
#define PTCH_DFN(type, name, ...)                  \
	typedef type (*name##_t)(PW_DELEGATE_BY_ARG_COUNT(_PTCH_FUNC_TYPE_ARGS_, __VA_ARGS__));     \
	volatile name##_t name##_ptch = NULL
#else
#define PTCH_DFN(type, name, ...)

#endif

#if defined(BUILD_ROM) && (BUILD_ROM)
#define PTCH_FST(type, name, ...)                  \
                if(name##_ptch) return name##_ptch(PW_DELEGATE_BY_ARG_COUNT(_PTCH_FUNC_CALL_ARGS_, __VA_ARGS__))
#else
#define PTCH_FST(type, name, ...)

#endif



#define PTCH_BND(type, name, ...)                  \
	typedef type (*name##_t)(PW_DELEGATE_BY_ARG_COUNT(_PTCH_FUNC_TYPE_ARGS_, __VA_ARGS__));     \
	extern name##_t name##_ptch;                   \
	name##_ptch

#endif /* INCLUDE_PATCH_H_ */
