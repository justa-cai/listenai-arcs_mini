#ifndef __LSF_CONFIG_H__
#define __LSF_CONFIG_H__

#ifdef __cplusplus
extern "C" {
#endif

// 核间属性 group ID 最大值. 必须AP/CP对等
#define LSF_PROPERTY_GROUP_COUNT    (16)

// 跨核内存管理器配置: 共享内存大小, AP->CP, AP<-CP. 32bytes对齐.
#define LSF_IC_ALLOCATOR_MEMSIZE    (32*100)

// 内存池配置: LsfPropertyParcel 最大共存数
#define LSF_PARCEL_MEMPOOL_CAPACITY	(8)

// 内存池配置: LsfVariant 最大共存数
#define LSF_VARIANT_MEMPOOL_CAPACITY    (8)

#ifdef __cplusplus
}
#endif

#endif /* __LSF_CONFIG_H__ */
