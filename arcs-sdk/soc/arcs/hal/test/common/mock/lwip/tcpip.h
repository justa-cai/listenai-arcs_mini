#ifndef LWIP_TCPIP_H
#define LWIP_TCPIP_H

#include <stdint.h>
#include <stddef.h>

typedef int err_t;
typedef uint8_t u8_t;
typedef uint16_t u16_t;

#define ERR_OK 0
#define ERR_MEM -1
#define ERR_BUF -2
#define ERR_VAL -3
#define ERR_IF -4
#define ERR_WOULDBLOCK -5

#define LWIP_NETIF_HOSTNAME 1
#define LWIP_IGMP 1
#define LWIP_IPV4 1
#define LWIP_DHCP 1
#define LWIP_DNS 1
#define LWIP_SO_RCVBUF 1

#define PBUF_LINK_ENCAPSULATION_HLEN 400
#define PBUF_FLAG_IS_CUSTOM 0x10

#ifndef LWIP_FUNC_ATTR
#define LWIP_FUNC_ATTR
#endif

#ifndef LWIP_FUNC_ALIGN
#define LWIP_FUNC_ALIGN
#endif

uint16_t lwip_htons_impl(uint16_t v);

static inline uint16_t lwip_htons(uint16_t v)
{
    return lwip_htons_impl(v);
}

static inline uint16_t htons(uint16_t v)
{
    return lwip_htons(v);
}

static inline uint16_t ntohs(uint16_t v)
{
    return lwip_htons(v);
}

typedef enum {
    PBUF_RAW = 0,
    PBUF_RAW_TX = 1,
    PBUF_LINK = 2
} pbuf_layer;

typedef enum {
    PBUF_RAM = 0,
    PBUF_REF = 1
} pbuf_type;

struct pbuf;
struct netif;

typedef err_t (*netif_init_fn)(struct netif *);
typedef err_t (*netif_input_fn)(struct pbuf *, struct netif *);

typedef struct {
    uint32_t addr;
} ip4_addr_t;

typedef struct {
    uint32_t addr;
} ip_addr_t;

typedef void (*pbuf_free_custom_fn)(struct pbuf *);

struct pbuf {
    void *payload;
    uint16_t len;
    uint16_t tot_len;
    struct pbuf *next;
    uint8_t flags;
};

struct pbuf_custom {
    struct pbuf pbuf;
    pbuf_free_custom_fn custom_free_function;
};

struct netif {
    char name[2];
    char num;
    const char *hostname;
    uint8_t hwaddr_len;
    uint8_t hwaddr[6];
    uint16_t mtu;
    uint8_t flags;
    void *state;
    err_t (*input)(struct pbuf *, struct netif *);
    err_t (*linkoutput)(struct netif *, struct pbuf *);
    err_t (*output)(struct netif *, struct pbuf *);
};

#define NETIF_FLAG_BROADCAST 0x01
#define NETIF_FLAG_ETHARP 0x02
#define NETIF_FLAG_LINK_UP 0x04
#define NETIF_FLAG_IGMP 0x08

void tcpip_init(void (*init)(void *), void *arg);
err_t tcpip_input(struct pbuf *p, struct netif *netif);

struct pbuf *pbuf_alloc(pbuf_layer layer, u16_t length, pbuf_type type);
struct pbuf *pbuf_alloc_wait(pbuf_layer layer, u16_t length, pbuf_type type);
struct pbuf *pbuf_alloced_custom(pbuf_layer layer, u16_t length, pbuf_type type,
                                 struct pbuf_custom *p, void *payload, u16_t payload_len);
u8_t pbuf_header(struct pbuf *p, int16_t header_size);
u8_t pbuf_free(struct pbuf *p);
void pbuf_ref(struct pbuf *p);
void pbuf_cat(struct pbuf *h, struct pbuf *t);

struct netif *netif_find(const char *name);
int netif_is_up(struct netif *netif);
int netif_is_link_up(struct netif *netif);
const ip4_addr_t *netif_ip4_addr(struct netif *netif);
const ip4_addr_t *netif_ip4_netmask(struct netif *netif);
const ip4_addr_t *netif_ip4_gw(struct netif *netif);
void netif_set_addr(struct netif *netif, const ip4_addr_t *ipaddr,
                    const ip4_addr_t *netmask, const ip4_addr_t *gw);

int dhcp_supplied_address(struct netif *netif);
void dhcps_start(struct netif *netif);
void dhcps_stop(void);

#endif
