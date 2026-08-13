#if CONFIG_IPC_CIDU
#define _IPC_CIDU_SHMEM_END (CONFIG_IPC_CIDU_SHMEM_BASE + CONFIG_IPC_CIDU_SHMEM_SIZE)

ASSERT((CONFIG_IPC_CIDU_SHMEM_BASE % 32) == 0,
    "error: IPC_CIDU shared memory base must be 32-byte aligned")
ASSERT((CONFIG_IPC_CIDU_SHMEM_SIZE % 32) == 0,
    "error: IPC_CIDU shared memory size must be 32-byte aligned")
ASSERT((__StackTop <= CONFIG_IPC_CIDU_SHMEM_BASE) || (__heap_start >= _IPC_CIDU_SHMEM_END),
    "error: IPC_CIDU shared memory overlaps normal RAM; adjust MEM_SRAM_* or IPC_CIDU_SHMEM_*")

#undef _IPC_CIDU_SHMEM_END
#endif
