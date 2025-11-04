/**
 *
 * @file sim_pbuf.c
 *
 * @brief Implementation of simulate socket
 *
 * Copyright (C) ListenAI 2020-2023
 *
 * Created on: Nov 11, 2023
 *
 *
 ****************************************************************************************
 */
#include <stdio.h>
#include <lwip/opt.h>
#include <lwip/pbuf.h>
#include <lwip/netif.h>
#include "sys_arch.h"

#define SIZEOF_STRUCT_PBUF        LWIP_MEM_ALIGN_SIZE(sizeof(struct pbuf))
/* Since the pool is created in memp, PBUF_POOL_BUFSIZE will be automatically
   aligned there. Therefore, PBUF_POOL_BUFSIZE_ALIGNED can be used here. */
#define PBUF_POOL_BUFSIZE_ALIGNED LWIP_MEM_ALIGN_SIZE(PBUF_POOL_BUFSIZE)

#define SIM_PBUF_NUM  8
#ifdef TX_BUF_COPY
#define SIM_PBUF_SIZE 384
#else
#define SIM_PBUF_SIZE 640
#endif
#define MEM_ALIGNMENT 4
#define SIM_MEM_ALIGN_BUFFER(addr)  ( (void *) (   ((uint32_t)(addr) + MEM_ALIGNMENT - 1) & ~(uint32_t)(MEM_ALIGNMENT-1))  )

#define PBUF_ALLOC_VIA_IPC 1

#if PBUF_ALLOC_VIA_IPC

#ifndef TX_BUF_COPY
struct sim_pbuf_mem
{
    uint8_t used;
    uint8_t mem[SIM_PBUF_SIZE] __ALIGN4;
};

static struct sim_pbuf_mem pbuf_pool[SIM_PBUF_NUM] __SHAREDRAMIPC;

static void
pbuf_init_alloced_pbuf(struct pbuf *p, void *payload, u16_t tot_len, u16_t len, pbuf_type type, u8_t flags);

static void *get_pbuf_mem(uint32_t size)
{
    int32_t i;
    struct sim_pbuf_mem *ptr;

    if (size > SIM_PBUF_SIZE)
        return NULL;
    for (i = 0; i < SIM_PBUF_NUM; i++)
    {
        ptr = &pbuf_pool[i];
        if (ptr->used == 0)
        {
            ptr->used = 1;
            return (void*)SIM_MEM_ALIGN_BUFFER(ptr->mem);
        }
    }

    return NULL;
}

struct pbuf *
pbuf_alloc_reference(void *payload, u16_t length, pbuf_type type)
{
  struct pbuf *p;
  SIMS_ASSERT("invalid pbuf_type", (type == PBUF_REF) || (type == PBUF_ROM));
  /* only allocate memory for the pbuf structure */
  p = (struct pbuf*)get_pbuf_mem(SIM_PBUF_SIZE);
  if (p == NULL) {
    SIMS_DEBUG("pbuf_alloc_reference: Could not allocate MEMP_PBUF for PBUF_%s.",
                (type == PBUF_ROM) ? "ROM" : "REF");
    return NULL;
  }
  pbuf_init_alloced_pbuf(p, payload, length, length, type, 0);
  return p;
}

struct pbuf* pbuf_alloc(pbuf_layer layer, uint16_t length, pbuf_type type)
{
    struct pbuf *p = NULL;
    u16_t offset;

    /* determine header offset */
    switch (layer) {
        case PBUF_LINK:
            /* add room for link layer header */
            offset = PBUF_LINK_ENCAPSULATION_HLEN + PBUF_LINK_HLEN;
            break;
        case PBUF_RAW_TX:
            /* add room for encapsulating link layer headers (e.g. 802.11) */
            offset = PBUF_LINK_ENCAPSULATION_HLEN;
            break;
        case PBUF_RAW:
            /* no offset (e.g. RX buffers or chain successors) */
            offset = 0;
            break;
        default:
            SIMS_ERR("pbuf_alloc: bad pbuf layer\n");
            return NULL;
    }
    switch (type) {
        case PBUF_REF: /* fall through */
          p = pbuf_alloc_reference(NULL, length, type);
          SIMS_DEBUG("pbuf_ref 0x%x\n", p);
          break;
        case PBUF_RAM:
            {
                uint32_t alloc_len = LWIP_MEM_ALIGN_SIZE(SIZEOF_STRUCT_PBUF + offset) + LWIP_MEM_ALIGN_SIZE(length);

                /* bug #50040: Check for integer overflow when calculating alloc_len */
                if (alloc_len < LWIP_MEM_ALIGN_SIZE(length)) {
                    return NULL;
                }

                /* If pbuf is to be allocated in RAM, allocate memory for it. */
                p = (struct pbuf*)get_pbuf_mem(alloc_len);
                SIMS_DEBUG("malloc length=%d\n", alloc_len);
#ifdef PBUF_DEBUG_TRACE
                pbuf_dbg_trace_record(PBUF_TRC_EVT_ALLOC, type, alloc_len, (uint32_t)p, __builtin_return_address(0));
#endif
            }

            if (p == NULL) {
                return NULL;
            }
            /* Set up internal structure of the pbuf. */
            p->payload = LWIP_MEM_ALIGN((void *)((u8_t *)p + SIZEOF_STRUCT_PBUF + offset));
            p->len = p->tot_len = length;
            p->next = NULL;
            p->type_internal = type;

            SIMS_ASSERT("pbuf_alloc: pbuf->payload properly aligned",
                    ((mem_ptr_t)p->payload % MEM_ALIGNMENT) == 0);
            break;
            /* pbuf references existing (non-volatile static constant) ROM payload? */
        default:
            SIMS_DEBUG("pbuf_alloc: erroneous type %d\n", type);
            return NULL;
    }
    /* set reference count */
    p->ref = 1;
    /* set flags */
    p->flags = PBUF_FLAG_IS_RTOS_MALLOC;

    SIMS_DEBUG("pbuf_alloc(length=%d layer=%d) 0x%x\n", length, layer, p);

    return p;
}

uint8_t pbuf_free(struct pbuf *p)
{
    struct sim_pbuf_mem *ptr;
    struct pbuf *q;

    while (p)
    {
        q = p->next;

        if ( ((int32_t)p < (int32_t)(&pbuf_pool[0])) || (((int32_t)p) >=  (int32_t)(&pbuf_pool[SIM_PBUF_NUM])))
        {
            SIMS_ERR("Invalid pbuf pointer.");
            return 0;
        }

        SIMS_ERR("pbuf_free 0x%x\n", p);

        ptr = (struct sim_pbuf_mem*)((uint32_t)p - (int32_t)(((struct sim_pbuf_mem*)0)->mem));
        ptr->used = 0;

        p = q;
    }
    return 1;
}
#else
struct pbuf* pbuf_alloc(pbuf_layer layer, uint16_t length, pbuf_type type)
{
    return NULL;
}

uint8_t pbuf_free(struct pbuf *p)
{
    return 0;
}
#endif //#ifndef TX_BUF_COPY

#else
struct pbuf* pbuf_alloc(pbuf_layer layer, uint16_t length, pbuf_type type)
{
    struct pbuf *pbuf = NULL;

    if (lwip_pbuf_alloc(&pbuf, layer, length, type))
    {
        return NULL;
    }
    /*The lwIP running on the other core will set the flag PBUF_FLAG_IPC when calling pbuf_alloc().
     * This causes fhost_tx_release_buf to send the pbuf to lwIP for release via IPC after TX completion.
     * So we clear the flag here. And the pbuf will be freed locally by calling pbuf_free()*/
    pbuf->flags &= ~PBUF_FLAG_IPC;

    return pbuf;
}

uint8_t pbuf_free(struct pbuf *p)
{
    struct pbuf *q, *raw = p;
    u8_t count, ready;

    if (p == NULL)
      return 0;

    count = 0;
    ready = 0;
    /* de-allocate all consecutive pbufs from the head of the chain that
     * obtain a zero reference count after decrementing*/
    while (p != NULL) {
      LWIP_PBUF_REF_T ref;
      SYS_ARCH_DECL_PROTECT(old_level);

      SYS_ARCH_PROTECT(old_level);

      ref = (p->ref) - 1;
      SYS_ARCH_UNPROTECT(old_level);
      if (ref == 0) {
        q = p->next;
#if LWIP_SUPPORT_CUSTOM_PBUF
        /* is this a custom pbuf? */
        if ((p->flags & PBUF_FLAG_IS_CUSTOM) != 0) {
          struct pbuf_custom *pc = (struct pbuf_custom *)p;
          pc->custom_free_function(p);
          pc->custom_free_function = NULL;
        } else
#endif /* LWIP_SUPPORT_CUSTOM_PBUF */
        {
            ready++;
        }
        count++;
        /* proceed to next pbuf */
        p = q;
      } else {
        p   = NULL;
        raw = NULL;
      }
    }

    if (ready && raw)
    {
        count = 0;
        lwip_pbuf_free(raw, &count);
    }

    return count;
}
#endif

void pbuf_cat(struct pbuf *h, struct pbuf *t)
{
    struct pbuf *p;

    if ((h == NULL) || (t == NULL))
        return;

    /* proceed to last pbuf of chain */
    for (p = h; p->next != NULL; p = p->next) {
        /* add total length of second chain to all totals of first chain */
        p->tot_len += t->tot_len;
    }
    /* { p is last pbuf of first h chain, p->next == NULL } */
    SIMS_ASSERT("p->tot_len == p->len (of last pbuf in chain)", p->tot_len == p->len);
    SIMS_ASSERT("p->next == NULL", p->next == NULL);
    /* add total length of second chain to last pbuf total of first chain */
    p->tot_len += t->tot_len;
    /* chain last pbuf of head (p) with first of tail (t) */
    p->next = t;
    /* p->next now references t, but the caller will drop its reference to t,
     * so netto there is no change to the reference count of t.
     */
}
#if 1
static u8_t
pbuf_add_header_impl(struct pbuf *p, size_t header_size_increment, u8_t force)
{
  u16_t type_internal;
  void *payload;
  u16_t increment_magnitude;

  LWIP_ASSERT("p != NULL", p != NULL);
  if ((p == NULL) || (header_size_increment > 0xFFFF)) {
    return 1;
  }
  if (header_size_increment == 0) {
    return 0;
  }

  increment_magnitude = (u16_t)header_size_increment;
  /* Do not allow tot_len to wrap as a result. */
  if ((u16_t)(increment_magnitude + p->tot_len) < increment_magnitude) {
    return 1;
  }

  type_internal = p->type_internal;

  /* pbuf types containing payloads? */
  if (type_internal & PBUF_TYPE_FLAG_STRUCT_DATA_CONTIGUOUS) {
    /* set new payload pointer */
    payload = (u8_t *)p->payload - header_size_increment;
    /* boundary check fails? */
    if ((u8_t *)payload < (u8_t *)p + SIZEOF_STRUCT_PBUF) {
      LWIP_DEBUGF( PBUF_DEBUG | LWIP_DBG_TRACE,
                   ("pbuf_add_header: failed as %p < %p (not enough space for new header size)\n",
                    (void *)payload, (void *)((u8_t *)p + SIZEOF_STRUCT_PBUF)));
      /* bail out unsuccessfully */
      return 1;
    }
    /* pbuf types referring to external payloads? */
  } else {
    /* hide a header in the payload? */
    if (force) {
      payload = (u8_t *)p->payload - header_size_increment;
    } else {
      /* cannot expand payload to front (yet!)
       * bail out unsuccessfully */
      return 1;
    }
  }
  LWIP_DEBUGF(PBUF_DEBUG | LWIP_DBG_TRACE, ("pbuf_add_header: old %p new %p (%"U16_F")\n",
              (void *)p->payload, (void *)payload, increment_magnitude));

  /* modify pbuf fields */
  p->payload = payload;
  p->len = (u16_t)(p->len + increment_magnitude);
  p->tot_len = (u16_t)(p->tot_len + increment_magnitude);


  return 0;
}
u8_t
pbuf_remove_header(struct pbuf *p, size_t header_size_decrement)
{
  void *payload;
  u16_t increment_magnitude;

  LWIP_ASSERT("p != NULL", p != NULL);
  if ((p == NULL) || (header_size_decrement > 0xFFFF)) {
    return 1;
  }
  if (header_size_decrement == 0) {
    return 0;
  }

  increment_magnitude = (u16_t)header_size_decrement;
  /* Check that we aren't going to move off the end of the pbuf */
  LWIP_ERROR("increment_magnitude <= p->len", (increment_magnitude <= p->len), return 1;);

  /* remember current payload pointer */
  payload = p->payload;
  LWIP_UNUSED_ARG(payload); /* only used in LWIP_DEBUGF below */

  /* increase payload pointer (guarded by length check above) */
  p->payload = (u8_t *)p->payload + header_size_decrement;
  /* modify pbuf length fields */
  p->len = (u16_t)(p->len - increment_magnitude);
  p->tot_len = (u16_t)(p->tot_len - increment_magnitude);

  LWIP_DEBUGF(PBUF_DEBUG | LWIP_DBG_TRACE, ("pbuf_remove_header: old %p new %p (%"U16_F")\n",
              (void *)payload, (void *)p->payload, increment_magnitude));

  return 0;
}
static u8_t
pbuf_header_impl(struct pbuf *p, s16_t header_size_increment, u8_t force)
{
  if (header_size_increment < 0) {
    return pbuf_remove_header(p, (size_t) - header_size_increment);
  } else {
    return pbuf_add_header_impl(p, (size_t)header_size_increment, force);
  }
}
uint8_t pbuf_header(struct pbuf *p, int16_t header_size_increment)
{
  return pbuf_header_impl(p, header_size_increment, 0);
}
/* Initialize members of struct pbuf after allocation */
static void
pbuf_init_alloced_pbuf(struct pbuf *p, void *payload, u16_t tot_len, u16_t len, pbuf_type type, u8_t flags)
{
  p->next = NULL;
  p->payload = payload;
  p->tot_len = tot_len;
  p->len = len;
  p->type_internal = (u8_t)type;
  p->flags = flags;
  p->ref = 1;
  p->if_idx = NETIF_NO_INDEX;
}
struct pbuf *
pbuf_alloced_custom(pbuf_layer l, u16_t length, pbuf_type type, struct pbuf_custom *p,
                    void *payload_mem, u16_t payload_mem_len)
{
  u16_t offset = (u16_t)l;
  void *payload;
  LWIP_DEBUGF(PBUF_DEBUG | LWIP_DBG_TRACE, ("pbuf_alloced_custom(length=%"U16_F")\n", length));

  if (LWIP_MEM_ALIGN_SIZE(offset) + length > payload_mem_len) {
    LWIP_DEBUGF(PBUF_DEBUG | LWIP_DBG_LEVEL_WARNING, ("pbuf_alloced_custom(length=%"U16_F") buffer too short\n", length));
    return NULL;
  }

  if (payload_mem != NULL) {
    payload = (u8_t *)payload_mem + LWIP_MEM_ALIGN_SIZE(offset);
  } else {
    payload = NULL;
  }
  pbuf_init_alloced_pbuf(&p->pbuf, payload, length, length, type, PBUF_FLAG_IS_CUSTOM);
  return &p->pbuf;
}
#else
static uint8_t sim_pbuf_header_impl(struct pbuf *p, int16_t header_size_increment, uint8_t force)
{
    u16_t type;
    void *payload;
    u16_t increment_magnitude;

    SIMS_ASSERT("p != NULL", p != NULL);
    if ((header_size_increment == 0) || (p == NULL)) {
        return 0;
    }

    if (header_size_increment < 0) {
        increment_magnitude = (u16_t)-header_size_increment;
        /* Check that we aren't going to move off the end of the pbuf */
        if (increment_magnitude > p->len)
            return 1;
    } else {
        increment_magnitude = (u16_t)header_size_increment;
#if 0
        /* Can't assert these as some callers speculatively call
           pbuf_header() to see if it's OK.  Will return 1 below instead. */
        /* Check that we've got the correct type of pbuf to work with */
        LWIP_ASSERT("p->type == PBUF_RAM || p->type == PBUF_POOL",
                p->type == PBUF_RAM || p->type == PBUF_POOL);
        /* Check that we aren't going to move off the beginning of the pbuf */
        LWIP_ASSERT("p->payload - increment_magnitude >= p + SIZEOF_STRUCT_PBUF",
                (u8_t *)p->payload - increment_magnitude >= (u8_t *)p + SIZEOF_STRUCT_PBUF);
#endif
    }

    type = p->type;
    /* remember current payload pointer */
    payload = p->payload;

    /* pbuf types containing payloads? */
    if (type == PBUF_RAM || type == PBUF_POOL) {
        /* set new payload pointer */
        p->payload = (u8_t *)p->payload - header_size_increment;
        /* boundary check fails? */
        if ((u8_t *)p->payload < (u8_t *)p + SIZEOF_STRUCT_PBUF) {
            SIMS_DEBUG("pbuf_header: failed as %p < %p (not enough space for new header size)\n",
                    (void *)p->payload, (void *)((u8_t *)p + SIZEOF_STRUCT_PBUF));
            /* restore old payload pointer */
            p->payload = payload;
            /* bail out unsuccessfully */
            return 1;
        }
        /* pbuf types referring to external payloads? */
    } else if (type == PBUF_REF || type == PBUF_ROM) {
        /* hide a header in the payload? */
        if ((header_size_increment < 0) && (increment_magnitude <= p->len)) {
            /* increase payload pointer */
            p->payload = (u8_t *)p->payload - header_size_increment;
        } else if ((header_size_increment > 0) && force) {
            p->payload = (u8_t *)p->payload - header_size_increment;
        } else {
            /* cannot expand payload to front (yet!)
             * bail out unsuccessfully */
            return 1;
        }
    } else {
        /* Unknown type */
        SIMS_ASSERT("bad pbuf type", 0);
        return 1;
    }
    /* modify pbuf length fields */
    p->len += header_size_increment;
    p->tot_len += header_size_increment;

    SIMS_DEBUG("pbuf_header: old %p new %p (%"S16_F")\n",
            (void *)payload, (void *)p->payload, header_size_increment);

    return 0;
}

uint8_t pbuf_header(struct pbuf *p, int16_t header_size_increment)
{
    return sim_pbuf_header_impl(p, header_size_increment, 0);
}
#endif
