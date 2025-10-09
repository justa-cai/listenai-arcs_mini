/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#include <string.h>

extern "C" {
  extern void exram_free(void *ptr);
  extern void *exram_calloc(size_t align, size_t num, size_t size);
  extern void *exram_malloc(size_t align, size_t size);
  extern void *exram_realloc(void *ptr, size_t size);
}

void *operator new(size_t count)
{
  return exram_malloc(4, count);
}

void *operator new[](size_t count)
{
  return exram_malloc(4, count);
}

void operator delete(void *ptr)
{
  exram_free(ptr);
}

void operator delete(void *ptr, size_t)
{
  operator delete(ptr);
}

void operator delete[](void *ptr)
{
  exram_free(ptr);
}

void operator delete[](void *ptr, size_t)
{
  operator delete[](ptr);
}
