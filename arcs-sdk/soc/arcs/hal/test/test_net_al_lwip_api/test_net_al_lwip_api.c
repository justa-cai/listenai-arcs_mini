#include "unity.h"
#include "fff.h"

#include <stdint.h>
#include <string.h>

#include "mock_lwip.h"
#include "mock_netif.h"
#include "mock_rtos.h"
#include "net_al.h"

DEFINE_FFF_GLOBALS;

static struct netif test_netif;
static ip4_addr_t ip_addr;
static struct pbuf test_pbuf;
static uint8_t payload_buf[64];
static struct netif g_netif_storage;

static void *rtos_calloc_custom(size_t count, size_t size)
{
    (void)count;
    (void)size;
    return &g_netif_storage;
}

void setUp(void)
{
    FFF_RESET_HISTORY();

    mock_lwip_reset();
    mock_netif_reset();
    mock_rtos_reset();

    memset(&test_netif, 0, sizeof(test_netif));
    memset(&test_pbuf, 0, sizeof(test_pbuf));
    memset(&g_netif_storage, 0, sizeof(g_netif_storage));
    memset(payload_buf, 0, sizeof(payload_buf));

    rtos_calloc_fake.custom_fake = rtos_calloc_custom;
}

void tearDown(void)
{
}

static void test_net_l2_socket_create_uses_lwip_api(void)
{
    lwip_socket_fake.return_val = 3;
    lwip_getsockopt_fake.return_val = 0;

    int sock = net_l2_socket_create(&test_netif, 0x1234);

    TEST_ASSERT_EQUAL(3, sock);
    TEST_ASSERT_EQUAL(1, lwip_socket_fake.call_count);
    TEST_ASSERT_EQUAL(1, lwip_getsockopt_fake.call_count);
    TEST_ASSERT_EQUAL(0, socket_fake.call_count);
    TEST_ASSERT_EQUAL(0, getsockopt_fake.call_count);
}

static void test_net_ip_chksum_uses_lwip_standard(void)
{
    lwip_standard_chksum_fake.return_val = 0xBEEF;

    uint16_t res = net_ip_chksum(payload_buf, 8);

    TEST_ASSERT_EQUAL_UINT16(0xBEEF, res);
    TEST_ASSERT_EQUAL(1, lwip_standard_chksum_fake.call_count);
}


static void test_net_l2_socket_create_failure_uses_lwip_close(void)
{
    lwip_socket_fake.return_val = 4;
    lwip_getsockopt_fake.return_val = -1;

    int sock = net_l2_socket_create(&test_netif, 0x2345);

    TEST_ASSERT_EQUAL(-1, sock);
    TEST_ASSERT_EQUAL(1, lwip_close_fake.call_count);
    TEST_ASSERT_EQUAL(0, close_fake.call_count);
}

static void test_net_l2_socket_delete_uses_lwip_close(void)
{
    lwip_socket_fake.return_val = 5;
    lwip_getsockopt_fake.return_val = 0;

    int sock = net_l2_socket_create(&test_netif, 0x3456);
    TEST_ASSERT_EQUAL(5, sock);

    int ret = net_l2_socket_delete(sock);

    TEST_ASSERT_EQUAL(0, ret);
    TEST_ASSERT_EQUAL(1, lwip_close_fake.call_count);
    TEST_ASSERT_EQUAL(0, close_fake.call_count);
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_net_l2_socket_create_uses_lwip_api);
    RUN_TEST(test_net_l2_socket_create_failure_uses_lwip_close);
    RUN_TEST(test_net_l2_socket_delete_uses_lwip_close);
    RUN_TEST(test_net_ip_chksum_uses_lwip_standard);

    return UNITY_END();
}
