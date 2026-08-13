/*
 * VenusA SRAM 布局静态校验
 *
 * 默认布局按芯片 SRAM 低地址窗口划分：
 *   - CIDU IPC：双核共享邮箱，默认 [0x20000000, 0x20001000)
 *   - LUNA_RAM：libluna.a 共享段，默认 [0x20001000, 0x20009000)
 *   - AP SRAM：AP 普通运行内存，默认 [0x20009000, 0x20016000)
 *   - CP SRAM：CP 普通运行内存，默认 [0x20016000, 0x20020000)
 *   - APRAM：算法工作内存，默认 [0x20020000, 0x20080000)
 *   - LUNA_PRIV_RAM：LUNA 核专属内存，默认从 0x20080000 开始
 *
 * AP/CP 镜像独立链接，MEM_SRAM_BASE/SIZE 只表示当前镜像 SRAM。
 * 这里不检查 AP SRAM 与 CP SRAM 是否互相重叠；调整分区时双镜像
 * 工程必须保证AP/CP prj.conf 使用一致的分区约定。
 *
 * 这里检查当前镜像 SRAM、LUNA 共享区、IPC、算法区、LUNA 专属区的
 * 合法范围和重叠关系，避免 prj.conf 调整分区后生成不可运行镜像。
 */

/* 判断两个半开区间 [base, base + size) 是否完全不重叠。 */
#define _END(base, size) ((base) + (size))
#define _NO_OVERFLOW(base, size) (_END(base, size) >= (base))
#define _DISJOINT(a_base, a_size, b_base, b_size) \
    ((_END(a_base, a_size) <= (b_base)) || ((a_base) >= _END(b_base, b_size)))

/* 当前镜像普通 SRAM 必须位于低 128KB CPU SRAM 窗口内。 */
ASSERT(_NO_OVERFLOW(CONFIG_MEM_SRAM_BASE, CONFIG_MEM_SRAM_SIZE),
    "error: VenusA SRAM address overflow")
ASSERT((CONFIG_MEM_SRAM_BASE >= 0x20000000),
    "error: VenusA SRAM base must be >= 0x20000000")
ASSERT((_END(CONFIG_MEM_SRAM_BASE, CONFIG_MEM_SRAM_SIZE) <= 0x20020000),
    "error: VenusA SRAM must end <= 0x20020000")

/*
 * LUNA 共享段允许在低 128KB 内按 prj.conf 调整，不能和当前镜像
 * SRAM、IPC、算法 APRAM、LUNA 专属区重叠。
 */
#if defined(CONFIG_MEM_LUNA_SHARED_SIZE) && CONFIG_MEM_LUNA_SHARED_SIZE > 0
ASSERT(_NO_OVERFLOW(CONFIG_MEM_LUNA_SHARED_BASE, CONFIG_MEM_LUNA_SHARED_SIZE),
    "error: VenusA LUNA shared memory address overflow")
ASSERT((CONFIG_MEM_LUNA_SHARED_BASE >= 0x20000000),
    "error: VenusA LUNA shared memory base must be >= 0x20000000")
ASSERT((_END(CONFIG_MEM_LUNA_SHARED_BASE, CONFIG_MEM_LUNA_SHARED_SIZE) <= 0x20020000),
    "error: VenusA LUNA shared memory must end <= 0x20020000")
ASSERT(_DISJOINT(CONFIG_MEM_SRAM_BASE, CONFIG_MEM_SRAM_SIZE,
                 CONFIG_MEM_LUNA_SHARED_BASE, CONFIG_MEM_LUNA_SHARED_SIZE),
    "error: VenusA SRAM overlaps LUNA shared memory, check prj.conf")
#endif

/*
 * 算法 APRAM 必须位于 0x20020000~0x20080000 的 384KB 算法窗口内。
 * 同时不能和当前镜像 SRAM、LUNA 共享段、IPC、LUNA 专属区重叠。
 */
#if defined(CONFIG_MEM_APRAM_SIZE) && CONFIG_MEM_APRAM_SIZE > 0
ASSERT(_NO_OVERFLOW(CONFIG_MEM_APRAM_BASE, CONFIG_MEM_APRAM_SIZE),
    "error: VenusA algorithm RAM address overflow")
ASSERT((CONFIG_MEM_APRAM_BASE >= 0x20020000),
    "error: VenusA algorithm RAM base must be >= 0x20020000")
ASSERT((_END(CONFIG_MEM_APRAM_BASE, CONFIG_MEM_APRAM_SIZE) <= 0x20080000),
    "error: VenusA algorithm RAM must end <= 0x20080000")
ASSERT(_DISJOINT(CONFIG_MEM_SRAM_BASE, CONFIG_MEM_SRAM_SIZE,
                 CONFIG_MEM_APRAM_BASE, CONFIG_MEM_APRAM_SIZE),
    "error: VenusA SRAM overlaps algorithm RAM, check prj.conf")
#if defined(CONFIG_MEM_LUNA_SHARED_SIZE) && CONFIG_MEM_LUNA_SHARED_SIZE > 0
ASSERT(_DISJOINT(CONFIG_MEM_LUNA_SHARED_BASE, CONFIG_MEM_LUNA_SHARED_SIZE,
                 CONFIG_MEM_APRAM_BASE, CONFIG_MEM_APRAM_SIZE),
    "error: VenusA LUNA shared memory overlaps algorithm RAM, check prj.conf")
#endif
#endif

/*
 * LUNA_PRIV_RAM 是 LUNA 核专属内存，不能被 AP/CP 普通 SRAM、
 * LUNA 共享段、IPC 或算法 APRAM 占用。
 */
#if defined(CONFIG_MEM_LUNA_BASE) && defined(CONFIG_MEM_LUNA_SIZE) && CONFIG_MEM_LUNA_SIZE > 0
ASSERT(_NO_OVERFLOW(CONFIG_MEM_LUNA_BASE, CONFIG_MEM_LUNA_SIZE),
    "error: VenusA LUNA private memory address overflow")
ASSERT((CONFIG_MEM_LUNA_BASE >= 0x20080000),
    "error: VenusA LUNA private memory base must be >= 0x20080000")
ASSERT(_DISJOINT(CONFIG_MEM_SRAM_BASE, CONFIG_MEM_SRAM_SIZE,
                 CONFIG_MEM_LUNA_BASE, CONFIG_MEM_LUNA_SIZE),
    "error: VenusA SRAM overlaps LUNA private memory, check prj.conf")
#if defined(CONFIG_MEM_LUNA_SHARED_SIZE) && CONFIG_MEM_LUNA_SHARED_SIZE > 0
ASSERT(_DISJOINT(CONFIG_MEM_LUNA_SHARED_BASE, CONFIG_MEM_LUNA_SHARED_SIZE,
                 CONFIG_MEM_LUNA_BASE, CONFIG_MEM_LUNA_SIZE),
    "error: VenusA LUNA shared memory overlaps LUNA private memory, check prj.conf")
#endif
#if defined(CONFIG_MEM_APRAM_SIZE) && CONFIG_MEM_APRAM_SIZE > 0
ASSERT(_DISJOINT(CONFIG_MEM_APRAM_BASE, CONFIG_MEM_APRAM_SIZE,
                 CONFIG_MEM_LUNA_BASE, CONFIG_MEM_LUNA_SIZE),
    "error: VenusA algorithm RAM overlaps LUNA private memory, check prj.conf")
#endif
#endif

/*
 * 布局级 IPC 共享内存允许在低 128KB 内按 prj.conf 调整，不能和
 * 当前镜像 SRAM、LUNA 共享段、算法 APRAM、LUNA 专属区重叠。
 */
#if defined(CONFIG_MEM_IPC_ISOLATED) && CONFIG_MEM_IPC_ISOLATED && CONFIG_MEM_IPC_SIZE > 0
ASSERT(_NO_OVERFLOW(CONFIG_MEM_IPC_BASE, CONFIG_MEM_IPC_SIZE),
    "error: VenusA IPC shared memory address overflow")
ASSERT((CONFIG_MEM_IPC_BASE >= 0x20000000),
    "error: VenusA IPC shared memory base must be >= 0x20000000")
ASSERT((_END(CONFIG_MEM_IPC_BASE, CONFIG_MEM_IPC_SIZE) <= 0x20020000),
    "error: VenusA IPC shared memory must end <= 0x20020000")
ASSERT(_DISJOINT(CONFIG_MEM_SRAM_BASE, CONFIG_MEM_SRAM_SIZE,
                 CONFIG_MEM_IPC_BASE, CONFIG_MEM_IPC_SIZE),
    "error: VenusA SRAM overlaps isolated IPC region, check prj.conf")
#if defined(CONFIG_MEM_LUNA_SHARED_SIZE) && CONFIG_MEM_LUNA_SHARED_SIZE > 0
ASSERT(_DISJOINT(CONFIG_MEM_LUNA_SHARED_BASE, CONFIG_MEM_LUNA_SHARED_SIZE,
                 CONFIG_MEM_IPC_BASE, CONFIG_MEM_IPC_SIZE),
    "error: VenusA LUNA shared memory overlaps isolated IPC region, check prj.conf")
#endif
#if defined(CONFIG_MEM_APRAM_SIZE) && CONFIG_MEM_APRAM_SIZE > 0
ASSERT(_DISJOINT(CONFIG_MEM_APRAM_BASE, CONFIG_MEM_APRAM_SIZE,
                 CONFIG_MEM_IPC_BASE, CONFIG_MEM_IPC_SIZE),
    "error: VenusA algorithm RAM overlaps isolated IPC region, check prj.conf")
#endif
#if defined(CONFIG_MEM_LUNA_BASE) && defined(CONFIG_MEM_LUNA_SIZE) && CONFIG_MEM_LUNA_SIZE > 0
ASSERT(_DISJOINT(CONFIG_MEM_IPC_BASE, CONFIG_MEM_IPC_SIZE,
                 CONFIG_MEM_LUNA_BASE, CONFIG_MEM_LUNA_SIZE),
    "error: VenusA IPC shared memory overlaps LUNA private memory, check prj.conf")
#endif
#endif

/*
 * CIDU IPC 共享邮箱由 AP/CP 两侧共同访问，允许在低 128KB 内按
 * prj.conf 调整。启用 MEM_IPC_ISOLATED 时，CIDU mailbox 必须位于
 * 布局级 IPC 共享内存区域内。
 */
#if defined(CONFIG_IPC_CIDU) && CONFIG_IPC_CIDU && defined(CONFIG_IPC_CIDU_SHMEM_SIZE) && CONFIG_IPC_CIDU_SHMEM_SIZE > 0
ASSERT(_NO_OVERFLOW(CONFIG_IPC_CIDU_SHMEM_BASE, CONFIG_IPC_CIDU_SHMEM_SIZE),
    "error: VenusA CIDU IPC shared memory address overflow")
ASSERT((CONFIG_IPC_CIDU_SHMEM_BASE >= 0x20000000),
    "error: VenusA CIDU IPC shared memory base must be >= 0x20000000")
ASSERT((_END(CONFIG_IPC_CIDU_SHMEM_BASE, CONFIG_IPC_CIDU_SHMEM_SIZE) <= 0x20020000),
    "error: VenusA CIDU IPC shared memory must end <= 0x20020000")
#if defined(CONFIG_MEM_IPC_ISOLATED) && CONFIG_MEM_IPC_ISOLATED && CONFIG_MEM_IPC_SIZE > 0
ASSERT((CONFIG_IPC_CIDU_SHMEM_BASE >= CONFIG_MEM_IPC_BASE),
    "error: VenusA CIDU IPC shared memory must be inside MEM_IPC region")
ASSERT((_END(CONFIG_IPC_CIDU_SHMEM_BASE, CONFIG_IPC_CIDU_SHMEM_SIZE) <=
        _END(CONFIG_MEM_IPC_BASE, CONFIG_MEM_IPC_SIZE)),
    "error: VenusA CIDU IPC shared memory must be inside MEM_IPC region")
#endif
ASSERT(_DISJOINT(CONFIG_MEM_SRAM_BASE, CONFIG_MEM_SRAM_SIZE,
                 CONFIG_IPC_CIDU_SHMEM_BASE, CONFIG_IPC_CIDU_SHMEM_SIZE),
    "error: VenusA SRAM overlaps CIDU IPC shared memory, check prj.conf")
#if defined(CONFIG_MEM_LUNA_SHARED_SIZE) && CONFIG_MEM_LUNA_SHARED_SIZE > 0
ASSERT(_DISJOINT(CONFIG_MEM_LUNA_SHARED_BASE, CONFIG_MEM_LUNA_SHARED_SIZE,
                 CONFIG_IPC_CIDU_SHMEM_BASE, CONFIG_IPC_CIDU_SHMEM_SIZE),
    "error: VenusA LUNA shared memory overlaps CIDU IPC shared memory, check prj.conf")
#endif
#if defined(CONFIG_MEM_APRAM_SIZE) && CONFIG_MEM_APRAM_SIZE > 0
ASSERT(_DISJOINT(CONFIG_MEM_APRAM_BASE, CONFIG_MEM_APRAM_SIZE,
                 CONFIG_IPC_CIDU_SHMEM_BASE, CONFIG_IPC_CIDU_SHMEM_SIZE),
    "error: VenusA algorithm RAM overlaps CIDU IPC shared memory, check prj.conf")
#endif
#if defined(CONFIG_MEM_LUNA_BASE) && defined(CONFIG_MEM_LUNA_SIZE) && CONFIG_MEM_LUNA_SIZE > 0
ASSERT(_DISJOINT(CONFIG_IPC_CIDU_SHMEM_BASE, CONFIG_IPC_CIDU_SHMEM_SIZE,
                 CONFIG_MEM_LUNA_BASE, CONFIG_MEM_LUNA_SIZE),
    "error: VenusA CIDU IPC shared memory overlaps LUNA private memory, check prj.conf")
#endif
#endif

#undef _END
#undef _NO_OVERFLOW
#undef _DISJOINT
