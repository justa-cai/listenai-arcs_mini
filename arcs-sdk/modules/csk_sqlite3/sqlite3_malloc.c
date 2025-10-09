
#if (CONFIG_CSK_SQLITE_SDK_CSK6)

#ifdef CONFIG_CSK_SQLITE_CSK_MEM
#include <csk_malloc.h>
static void* __sq_malloc(uint32_t size){

  return csk_malloc(size);
}

static void* __sq_realloc(void* ptr,uint32_t sz){

    return csk_realloc( ptr, sz);
}

static void __sq_free(void* ptr){

  csk_free(ptr);

}

#else
extern void *k_malloc(size_t size);
extern void *k_calloc(size_t nmemb, size_t size);
extern void k_free(void *ptr);
static void *k_realloc(void *ptr, size_t size)
{
	void *ptr_new = NULL;
	if(ptr == NULL){
		ptr_new = k_calloc(1, size);
	}else if(size == 0){
		k_free(ptr);
		return NULL;
	}else {
		ptr_new = k_calloc(1, size);
	}

	if(ptr_new && ptr){
		// fixme: 这里对 ptr 进行拷贝，有可能存在地址越界的问题。暂时没有找到ptr指针长度的方法
		memcpy(ptr_new, ptr, size);
		k_free(ptr);
	}

	return ptr_new;
}

static void* __sq_malloc(uint32_t size){

  return k_malloc(size);
}

static void* __sq_realloc(void* ptr,uint32_t sz){

    return k_realloc( ptr, sz);
}

static void __sq_free(void* ptr){

  k_free(ptr);

}
#endif
#elif (CONFIG_CSK_SQLITE_SDK_ARCS)
//use psram
#include "esp_heap_caps.h"

#if (CONFIG_CSK_SQLITE_ARCS_MEM_RAM)
static void* __sq_malloc(uint32_t size){

  return heap_caps_malloc(size, MALLOC_CAP_INTERNAL | MALLOC_CAP_32BIT);
}

static void* __sq_realloc(void* ptr,uint32_t sz){

    return heap_caps_realloc( ptr, sz, MALLOC_CAP_INTERNAL | MALLOC_CAP_32BIT);
}

static void __sq_free(void* ptr){

  heap_caps_free(ptr);

}
#else
static void* __sq_malloc(uint32_t size){

  return heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_32BIT);
}

static void* __sq_realloc(void* ptr,uint32_t sz){

    return heap_caps_realloc( ptr, sz, MALLOC_CAP_SPIRAM | MALLOC_CAP_32BIT);
}

static void __sq_free(void* ptr){

  heap_caps_free(ptr);

}
#endif
#else
#error "Unsupport the sdk type"
#endif




