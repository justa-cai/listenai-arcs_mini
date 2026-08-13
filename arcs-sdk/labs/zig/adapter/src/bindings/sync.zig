///! ARCS SDK 同步原语 FFI 绑定
///! 映射 lisa_mutex.h, lisa_semaphore.h, lisa_queue.h

// ════════════════════════════════════════════════════════════════════
// Mutex (lisa_mutex.h)
// ════════════════════════════════════════════════════════════════════
pub const MutexHandle = ?*anyopaque;

pub extern fn lisa_mutex_create() callconv(.C) MutexHandle;
pub extern fn lisa_mutex_lock(mutex: MutexHandle, block_time: i32) callconv(.C) i32;
pub extern fn lisa_mutex_unlock(mutex: MutexHandle) callconv(.C) i32;
pub extern fn lisa_mutex_delete(mutex: MutexHandle) callconv(.C) i32;

// ════════════════════════════════════════════════════════════════════
// Semaphore (lisa_semaphore.h)
// ════════════════════════════════════════════════════════════════════
pub const SemaphoreHandle = ?*anyopaque;

pub extern fn lisa_semaphore_create(count: u32) callconv(.C) SemaphoreHandle;
pub extern fn lisa_semaphore_take(sem: SemaphoreHandle, block_time: i32) callconv(.C) i32;
pub extern fn lisa_semaphore_give(sem: SemaphoreHandle) callconv(.C) i32;
pub extern fn lisa_semaphore_delete(sem: SemaphoreHandle) callconv(.C) i32;
pub extern fn lisa_semaphore_clear(sem: SemaphoreHandle) callconv(.C) i32;

// ════════════════════════════════════════════════════════════════════
// Queue (lisa_queue.h)
// ════════════════════════════════════════════════════════════════════
pub const QueueHandle = ?*anyopaque;

pub extern fn lisa_queue_create(count: u32, queue_name: ?[*]const u8, item_size: u32) callconv(.C) QueueHandle;
pub extern fn lisa_queue_push(queue: QueueHandle, item: *const anyopaque, item_size: u32, wait: i32) callconv(.C) i32;
pub extern fn lisa_queue_push_front(queue: QueueHandle, item: *const anyopaque, item_size: u32, wait: i32) callconv(.C) i32;
pub extern fn lisa_queue_pop(queue: QueueHandle, item: *anyopaque, item_size: u32, wait: i32) callconv(.C) i32;
pub extern fn lisa_queue_expand(queue: QueueHandle, expand_count: u32, item_size: u32) callconv(.C) i32;
pub extern fn lisa_queue_full(queue: QueueHandle) callconv(.C) bool;
pub extern fn lisa_queue_delete(queue: QueueHandle) callconv(.C) i32;
pub extern fn lisa_queue_waiting(queue: QueueHandle) callconv(.C) u32;
pub extern fn lisa_queue_clear(queue: QueueHandle) callconv(.C) i32;
pub extern fn lisa_queue_size(queue: QueueHandle) callconv(.C) u32;
