#include "cmd.h"

#include "shell.h"
#include "sysheap.h"
#include "multi_heap.h"

static void heap_travel_cb(void *start, void *end, multi_heap_info_t *info)
{
    shellPrint(shellGetCurrent(), "%p %p %12d %12d %12d %12d %12d %12d %12d\n", start, end, info->allocated_blocks,
               info->free_blocks, info->total_blocks, info->largest_free_block, info->total_allocated_bytes,
               info->total_free_bytes, info->minimum_free_bytes);
}

static int heap_cmd_handler(int argc, char **argv)
{
    shellPrint(shellGetCurrent(), "%10s %10s %12s %12s %12s %12s %12s %12s %12s\n", "[Start]", "[End]", "[Alloc/BK]",
               "[Free/BK]", "[Total/BK]", "[MaxFree/BK]", "[Alloc/B]", "[Free/B]", "[MinFree/B]");

    void heap_caps_travel(void (*callback)(void *, void *, multi_heap_info_t *));
    heap_caps_travel(heap_travel_cb);

    return 0;
}

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, heap,
                 heap_cmd_handler, heap info);
