#include <string.h>
#include "lis_algo.h"

#define MEM_DEBUG   0
#if MEM_DEBUG 
static long sram_allocated=0,psram_allocated=0;
#endif

// extern uint32_t get_time_us(void);

// uint32_t os_ticks_get(void)
// {
//     return get_time_us()/1000;
// }

//psram strdup
char *pendup(char *s)  
{
    if(s == NULL||*s==0)
        return NULL;
    uint32_t len = strlen(s) + 1;
    if(len>1024){
        LOGW("[pendup]big str,len>1k:%lu\n",len);
    }
    void *new = os_mem_alloc(len);
	if(new == NULL) return NULL;
	return (char *)memcpy(new, s, len);
}
