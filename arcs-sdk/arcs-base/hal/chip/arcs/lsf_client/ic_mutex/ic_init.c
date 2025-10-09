/*
 * Inter-cores synchronization primitives for multiprocessor system.
 * Copyright 2024 ListenAI
 */
#include <stdint.h>

#include "ic.h"
#include "ic_misc.h"
// #include "ic_mutex.h"
//#include "ic_mutex_internal.h"

#include "Driver_MBX.h"

// private functions
extern int IC_Mutex_Class_init(void);

#ifdef CONFIG_HAL_LSF_CLIENT
// client 端

// module: ic_stream
#include "ic_fence.h"
#include "ic_proxy.h"
#include "rpc_client.h"

// 全局: 核间资源. ICFence id: 0
static ICFenceHandle _IC_Mutex_fence_0;

// waitQueue内存池
//extern IC_Mutex_WaitQueue *_IC_Mutex_waitQueue_pool;

/*
 * Top level multi-core system initialization routine.
 *
 * This function needs to be called prior to creating all the inter-cores
 * primitives. It initializes the \a inter-cores interrupt handlers.
 *
 * \return     IC_OK if successful, else returns error code
 */
int IC_System_initialize()
{
  ic_lock_init();
  //--------------------------------------------------------------
  // rpc初始化, 以便挂接handler, 不启动
  RPC_Client_init();

  //--------------------------------------------------------------
  // 初始化对象发布service's proxy, 注册到rpc
  IC_Proxy_init();

  return 0;
}

/*
 * Top level multi-core system syncing routine.
 *
 * This is a barrier call that allows for all cores in the system to
 * synchronize.
 * All cores are synchronized after this call. This function needs to be called
 * prior to using all the inter-cores primitives.
 *
 * \return     IC_OK if successful, else returns error code
 */
int IC_System_sync()
{
  //--------------------------------------------------------------
  // rpc启动, 并与server同步
  RPC_Client_Start();

  //--------------------------------------------------------------
  // 核间通信子系统的核间同步

  // LSF
#if 0
  //--------------------------------------------------------------
  // IC_Mutex 主从同步
  // 核间原语对象的核间同步, 以确认对端已经就绪

  // 此时server端的ICFence对象已就绪
  _IC_Mutex_fence_0 = (ICFenceHandle) IC_Proxy_getRemoteFence(0);

  // 对象级的核间同步, 以确认对端已经就绪.
  ICFence_syncWithRemote(_IC_Mutex_fence_0);

  // Synchronize across all cores
  uint32_t waitQueue_pool;
  ICFence_wait(_IC_Mutex_fence_0, &waitQueue_pool);
  _IC_Mutex_waitQueue_pool = (IC_Mutex_WaitQueue *) waitQueue_pool;

  // 挂接中断处理: mbox->IC_Mutex
  IC_Mutex_linkInterrupt();
#endif
  return 0;
}

#elif defined(CONFIG_HAL_LSF_SERVER)
// server 端

// module: ic_stream
#include "ic_fence.h"
#include "ic_service.h"
#include "rpc_server.h"

// 全局: 核间资源. ICFence id: 0
static ICFenceHandle _IC_Mutex_fence_0;

// waitQueue内存池
//extern IC_Mutex_WaitQueue _IC_Mutex_waitQueue_pool[];

/*
 * Top level multi-core system initialization routine.
 *
 * This function needs to be called prior to creating all the inter-cores
 * primitives. It initializes the \a inter-cores interrupt handlers.
 *
 * \return     IC_OK if successful, else returns error code
 */
int IC_System_initialize()
{
  ic_lock_init();
  //--------------------------------------------------------------
  // rpc初始化, 以便挂接handler, 不启动
  RPC_Server_init();

  // Server proc initialize all inter-proc resources

  //--------------------------------------------------------------
  // CP侧: 内部占用的核间原语对象
#if 0
  // ICFence id:0 - IC_Mutex 实现内部已占用
  // 初始化对象, CP侧, 由ic_service发布
  _IC_Mutex_fence_0 = ICFence_Creator_createObj(0);
  IC_ASSERT(_IC_Mutex_fence_0 != NULL);
#endif
  return 0;
}

/*
 * Top level multi-core system syncing routine.
 *
 * This is a barrier call that allows for all cores in the system to
 * synchronize.
 * All cores are synchronized after this call. This function needs to be called
 * prior to using all the inter-cores primitives.
 *
 * \return     IC_OK if successful, else returns error code
 */
int IC_System_sync()
{
  //--------------------------------------------------------------
  // 之前: CP侧: 用户定制的核间原语对象, 注册到rpc, 被ic_service发布, 以接收调用

  //--------------------------------------------------------------
  // 初始化对象发布service, 放入核间原语对象, 注册到rpc, 以接收调用
  IC_Service_init();

  //--------------------------------------------------------------
  // rpc启动, 并与client同步
  RPC_Server_Start();

  //--------------------------------------------------------------
  // 核间通信子系统的核间同步

  // LSF
#if 0
  //--------------------------------------------------------------
  // IC_Mutex 主从同步
  // 核间原语对象的核间同步, 以确认对端已经就绪

  // 挂接中断处理: mbox->IC_Mutex
  IC_Mutex_linkInterrupt();

  // Synchronize across all cores
  // 对象级的核间同步, 以确认对端已经就绪.
  ICFence_syncWithRemote(_IC_Mutex_fence_0);

  // 同步 对象池 地址
  uint32_t waitQueue_pool = (uint32_t)(uintptr_t)_IC_Mutex_waitQueue_pool;
  ICFence_notify(_IC_Mutex_fence_0, waitQueue_pool);
#endif
  return 0;
}

#endif
