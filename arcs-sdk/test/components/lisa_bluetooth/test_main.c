/*
 * Unit tests for lisa_bluetooth callback mechanisms.
 *
 * Tests cover:
 * 1. enable_cmp callback
 * 2. BLE connection callback
 * 3. BLE disconnection callback
 * 4. BLE bond indication callback
 * 5. BLE key request callback
 *
 * Since lisa_bluetooth.c and bt_ble_user.c depend on the full BLE stack,
 * we extract and reproduce the minimal callback logic in isolation.
 */

/*=======Test Runner Used To Run Each Test Below=====*/
#define RUN_TEST(TestFunc, TestLineNum)                                                                                \
    {                                                                                                                  \
        Unity.CurrentTestName = #TestFunc;                                                                             \
        Unity.CurrentTestLineNumber = TestLineNum;                                                                     \
        Unity.NumberOfTests++;                                                                                         \
        if (TEST_PROTECT()) {                                                                                          \
            setUp();                                                                                                   \
            TestFunc();                                                                                                \
        }                                                                                                              \
        if (TEST_PROTECT()) {                                                                                          \
            tearDown();                                                                                                \
        }                                                                                                              \
        UnityConcludeTest();                                                                                           \
    }

/*=======Automagically Detected Files To Include=====*/

#include "unity.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* ---- Mock types (matching bt_stack_hal.h) ---- */

#define GAP_BD_ADDR_LEN  (6)

typedef struct gap_bdaddr {
    uint8_t addr[GAP_BD_ADDR_LEN];
    uint8_t addr_type;
} gap_bdaddr_t;

/* ---- Reproduce callback types and registration logic ---- */

typedef void (*lisa_bluetooth_enable_cmp_cb_t)(uint16_t status);
typedef void (*lisa_ble_conn_cb_t)(uint8_t conidx, uint16_t conhdl, const gap_bdaddr_t *peer_addr);
typedef void (*lisa_ble_disc_cb_t)(uint8_t conidx, uint16_t conhdl, uint16_t reason);
typedef void (*lisa_ble_bond_cb_t)(uint8_t conidx, uint8_t info, uint8_t value);
typedef void (*lisa_ble_key_req_cb_t)(uint8_t conidx, uint8_t key_type, uint32_t passkey);

/*
 * Simulates lisa_bluetooth.c internal state:
 * - s_enable_cmp_cb: set via lisa_bluetooth_init()
 * - s_ble_conn_cb / s_ble_disc_cb: set via register functions
 */
static lisa_bluetooth_enable_cmp_cb_t s_enable_cmp_cb = NULL;
static lisa_ble_conn_cb_t s_ble_conn_cb = NULL;
static lisa_ble_disc_cb_t s_ble_disc_cb = NULL;
static lisa_ble_bond_cb_t s_ble_bond_cb = NULL;
static lisa_ble_key_req_cb_t s_ble_key_req_cb = NULL;

int lisa_bluetooth_init(lisa_bluetooth_enable_cmp_cb_t cb)
{
    s_enable_cmp_cb = cb;
    return 0;
}

void lisa_ble_register_conn_cb(lisa_ble_conn_cb_t cb)
{
    s_ble_conn_cb = cb;
}

void lisa_ble_register_disc_cb(lisa_ble_disc_cb_t cb)
{
    s_ble_disc_cb = cb;
}

void lisa_ble_register_bond_cb(lisa_ble_bond_cb_t cb)
{
    s_ble_bond_cb = cb;
}

void lisa_ble_register_key_req_cb(lisa_ble_key_req_cb_t cb)
{
    s_ble_key_req_cb = cb;
}

/*
 * Mirrors lisa_ble_notify_* from lisa_bluetooth.c.
 * In real code these are called by bt_ble_user.c event handlers.
 */
void lisa_ble_notify_enable_cmp(uint16_t status)
{
    if (s_enable_cmp_cb) {
        s_enable_cmp_cb(status);
    }
}

void lisa_ble_notify_connected(uint8_t conidx, uint16_t conhdl, const gap_bdaddr_t *peer_addr)
{
    if (s_ble_conn_cb) {
        s_ble_conn_cb(conidx, conhdl, peer_addr);
    }
}

void lisa_ble_notify_disconnected(uint8_t conidx, uint16_t conhdl, uint16_t reason)
{
    if (s_ble_disc_cb) {
        s_ble_disc_cb(conidx, conhdl, reason);
    }
}

void lisa_ble_notify_bond(uint8_t conidx, uint8_t info, uint8_t value)
{
    if (s_ble_bond_cb) {
        s_ble_bond_cb(conidx, info, value);
    }
}

bool lisa_ble_notify_key_req(uint8_t conidx, uint8_t key_type, uint32_t passkey)
{
    if (s_ble_key_req_cb) {
        s_ble_key_req_cb(conidx, key_type, passkey);
        return true;
    }
    return false;
}

/* ---- Test helpers ---- */

static int      g_cb_called;
static uint16_t g_cb_status;

/* enable_cmp helpers */
static void test_callback(uint16_t status)
{
    g_cb_called++;
    g_cb_status = status;
}

static void test_callback_alt(uint16_t status)
{
    g_cb_called += 100;
    g_cb_status = status;
}

/* conn_ind helpers */
static uint8_t     g_conn_conidx;
static uint16_t    g_conn_conhdl;
static gap_bdaddr_t g_conn_peer_addr;

static void test_conn_cb(uint8_t conidx, uint16_t conhdl, const gap_bdaddr_t *peer_addr)
{
    g_cb_called++;
    g_conn_conidx = conidx;
    g_conn_conhdl = conhdl;
    if (peer_addr) {
        g_conn_peer_addr = *peer_addr;
    }
}

/* disc_ind helpers */
static uint8_t  g_disc_conidx;
static uint16_t g_disc_conhdl;
static uint16_t g_disc_reason;

/* bond_ind helpers */
static uint8_t  g_bond_conidx;
static uint8_t  g_bond_info;
static uint8_t  g_bond_value;

/* key_req helpers */
static uint8_t  g_key_req_conidx;
static uint8_t  g_key_req_type;
static uint32_t g_key_req_passkey;

static void test_disc_cb(uint8_t conidx, uint16_t conhdl, uint16_t reason)
{
    g_cb_called++;
    g_disc_conidx = conidx;
    g_disc_conhdl = conhdl;
    g_disc_reason = reason;
}

static void test_bond_cb(uint8_t conidx, uint8_t info, uint8_t value)
{
    g_cb_called++;
    g_bond_conidx = conidx;
    g_bond_info = info;
    g_bond_value = value;
}

static void test_key_req_cb(uint8_t conidx, uint8_t key_type, uint32_t passkey)
{
    g_cb_called++;
    g_key_req_conidx = conidx;
    g_key_req_type = key_type;
    g_key_req_passkey = passkey;
}

/* ========================================================================
 * enable_cmp callback tests (existing)
 * ======================================================================== */

void test_init_with_null_callback(void)
{
    lisa_bluetooth_init(NULL);
    lisa_ble_notify_enable_cmp(0);

    TEST_ASSERT_EQUAL_INT(0, g_cb_called);
    TEST_ASSERT_EQUAL_HEX16(0xFFFF, g_cb_status);
}

void test_init_callback_invoked_on_success(void)
{
    lisa_bluetooth_init(test_callback);
    lisa_ble_notify_enable_cmp(0);

    TEST_ASSERT_EQUAL_INT(1, g_cb_called);
    TEST_ASSERT_EQUAL_HEX16(0, g_cb_status);
}

void test_init_callback_invoked_on_failure(void)
{
    lisa_bluetooth_init(test_callback);
    lisa_ble_notify_enable_cmp(0x0005);

    TEST_ASSERT_EQUAL_INT(1, g_cb_called);
    TEST_ASSERT_EQUAL_HEX16(0x0005, g_cb_status);
}

void test_callback_invoked_multiple_times(void)
{
    lisa_bluetooth_init(test_callback);
    lisa_ble_notify_enable_cmp(0);
    lisa_ble_notify_enable_cmp(0);
    lisa_ble_notify_enable_cmp(0);

    TEST_ASSERT_EQUAL_INT(3, g_cb_called);
}

void test_reinit_replaces_callback(void)
{
    lisa_bluetooth_init(test_callback);
    lisa_bluetooth_init(test_callback_alt);
    lisa_ble_notify_enable_cmp(0);

    TEST_ASSERT_EQUAL_INT(100, g_cb_called);
}

void test_init_returns_zero(void)
{
    int ret = lisa_bluetooth_init(test_callback);
    TEST_ASSERT_EQUAL_INT(0, ret);
}

/* ========================================================================
 * BLE connection callback tests (new)
 * ======================================================================== */

/**
 * @brief No crash when conn callback is NULL and connection event fires
 */
void test_conn_cb_null_no_crash(void)
{
    lisa_ble_register_conn_cb(NULL);

    gap_bdaddr_t addr = { .addr = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66}, .addr_type = 0 };
    lisa_ble_notify_connected(0, 0x0001, &addr);

    TEST_ASSERT_EQUAL_INT(0, g_cb_called);
}

/**
 * @brief Connection callback receives correct conidx, conhdl, and peer address
 */
void test_conn_cb_params_passed_correctly(void)
{
    lisa_ble_register_conn_cb(test_conn_cb);

    gap_bdaddr_t addr = { .addr = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF}, .addr_type = 1 };
    lisa_ble_notify_connected(3, 0x0042, &addr);

    TEST_ASSERT_EQUAL_INT(1, g_cb_called);
    TEST_ASSERT_EQUAL_UINT8(3, g_conn_conidx);
    TEST_ASSERT_EQUAL_HEX16(0x0042, g_conn_conhdl);
    TEST_ASSERT_EQUAL_UINT8(0xAA, g_conn_peer_addr.addr[0]);
    TEST_ASSERT_EQUAL_UINT8(0xFF, g_conn_peer_addr.addr[5]);
    TEST_ASSERT_EQUAL_UINT8(1, g_conn_peer_addr.addr_type);
}

/**
 * @brief Connection callback can be replaced by re-registering
 */
void test_conn_cb_can_be_replaced(void)
{
    lisa_ble_register_conn_cb(test_conn_cb);
    /* Unregister by passing NULL */
    lisa_ble_register_conn_cb(NULL);

    gap_bdaddr_t addr = {0};
    lisa_ble_notify_connected(0, 0, &addr);

    TEST_ASSERT_EQUAL_INT(0, g_cb_called);
}

/**
 * @brief Connection callback fires multiple times for multiple connections
 */
void test_conn_cb_multiple_connections(void)
{
    lisa_ble_register_conn_cb(test_conn_cb);

    gap_bdaddr_t addr1 = { .addr = {0x01}, .addr_type = 0 };
    gap_bdaddr_t addr2 = { .addr = {0x02}, .addr_type = 0 };

    lisa_ble_notify_connected(0, 0x0001, &addr1);
    lisa_ble_notify_connected(1, 0x0002, &addr2);

    TEST_ASSERT_EQUAL_INT(2, g_cb_called);
    /* Last call wins for the captured values */
    TEST_ASSERT_EQUAL_UINT8(1, g_conn_conidx);
    TEST_ASSERT_EQUAL_UINT8(0x02, g_conn_peer_addr.addr[0]);
}

/* ========================================================================
 * BLE disconnection callback tests (new)
 * ======================================================================== */

/**
 * @brief No crash when disc callback is NULL and disconnection event fires
 */
void test_disc_cb_null_no_crash(void)
{
    lisa_ble_register_disc_cb(NULL);

    lisa_ble_notify_disconnected(0, 0x0001, 0x0013);

    TEST_ASSERT_EQUAL_INT(0, g_cb_called);
}

/**
 * @brief Disconnection callback receives correct conidx, conhdl, and reason
 */
void test_disc_cb_params_passed_correctly(void)
{
    lisa_ble_register_disc_cb(test_disc_cb);

    lisa_ble_notify_disconnected(2, 0x0099, 0x0008);

    TEST_ASSERT_EQUAL_INT(1, g_cb_called);
    TEST_ASSERT_EQUAL_UINT8(2, g_disc_conidx);
    TEST_ASSERT_EQUAL_HEX16(0x0099, g_disc_conhdl);
    TEST_ASSERT_EQUAL_HEX16(0x0008, g_disc_reason);
}

/**
 * @brief Common disconnect reasons are correctly propagated
 *   0x0013 = Remote User Terminated
 *   0x0008 = Connection Timeout
 *   0x003E = Connection Failed to be Established
 */
void test_disc_cb_various_reasons(void)
{
    lisa_ble_register_disc_cb(test_disc_cb);

    /* Remote user terminated */
    lisa_ble_notify_disconnected(0, 0x0001, 0x0013);
    TEST_ASSERT_EQUAL_HEX16(0x0013, g_disc_reason);

    /* Connection timeout */
    lisa_ble_notify_disconnected(0, 0x0001, 0x0008);
    TEST_ASSERT_EQUAL_HEX16(0x0008, g_disc_reason);

    /* Failed to establish */
    lisa_ble_notify_disconnected(0, 0x0001, 0x003E);
    TEST_ASSERT_EQUAL_HEX16(0x003E, g_disc_reason);

    TEST_ASSERT_EQUAL_INT(3, g_cb_called);
}

/**
 * @brief Disconnection callback can be unregistered by passing NULL
 */
void test_disc_cb_can_be_unregistered(void)
{
    lisa_ble_register_disc_cb(test_disc_cb);
    lisa_ble_register_disc_cb(NULL);

    lisa_ble_notify_disconnected(0, 0x0001, 0x0013);

    TEST_ASSERT_EQUAL_INT(0, g_cb_called);
}

/* ========================================================================
 * BLE bond callback tests (new)
 * ======================================================================== */

/**
 * @brief No crash when bond callback is NULL and bond event fires
 */
void test_bond_cb_null_no_crash(void)
{
    lisa_ble_register_bond_cb(NULL);

    lisa_ble_notify_bond(0, 0 /* GAP_PAIRING_SUCCEED */, 0);

    TEST_ASSERT_EQUAL_INT(0, g_cb_called);
}

/**
 * @brief Bond callback receives correct conidx, info, and value
 */
void test_bond_cb_params_passed_correctly(void)
{
    lisa_ble_register_bond_cb(test_bond_cb);

    /* Simulate GAP_PAIRING_SUCCEED (info=0, value=0) */
    lisa_ble_notify_bond(1, 0, 0);

    TEST_ASSERT_EQUAL_INT(1, g_cb_called);
    TEST_ASSERT_EQUAL_UINT8(1, g_bond_conidx);
    TEST_ASSERT_EQUAL_UINT8(0, g_bond_info);
    TEST_ASSERT_EQUAL_UINT8(0, g_bond_value);
}

/**
 * @brief Bond callback handles pairing failure
 */
void test_bond_cb_pairing_failed(void)
{
    lisa_ble_register_bond_cb(test_bond_cb);

    /* Simulate GAP_PAIRING_FAILED (info=1, value=error_code) */
    lisa_ble_notify_bond(0, 1, 0x05);

    TEST_ASSERT_EQUAL_INT(1, g_cb_called);
    TEST_ASSERT_EQUAL_UINT8(1, g_bond_info);
    TEST_ASSERT_EQUAL_UINT8(0x05, g_bond_value);
}

/**
 * @brief Bond callback handles link encryption event
 */
void test_bond_cb_link_encrypted(void)
{
    lisa_ble_register_bond_cb(test_bond_cb);

    /* Simulate GAP_LINK_ENCRYPTED (info=10) */
    lisa_ble_notify_bond(0, 10, 0);

    TEST_ASSERT_EQUAL_INT(1, g_cb_called);
    TEST_ASSERT_EQUAL_UINT8(10, g_bond_info);
}

/**
 * @brief Bond callback can be unregistered by passing NULL
 */
void test_bond_cb_can_be_unregistered(void)
{
    lisa_ble_register_bond_cb(test_bond_cb);
    lisa_ble_register_bond_cb(NULL);

    lisa_ble_notify_bond(0, 0, 0);

    TEST_ASSERT_EQUAL_INT(0, g_cb_called);
}

/* ========================================================================
 * BLE key request callback tests (new)
 * ======================================================================== */

/**
 * @brief No crash when key_req callback is NULL and key request fires
 */
void test_key_req_cb_null_no_crash(void)
{
    lisa_ble_register_key_req_cb(NULL);

    lisa_ble_notify_key_req(0, 0, 123456);

    TEST_ASSERT_EQUAL_INT(0, g_cb_called);
}

/**
 * @brief Key request callback receives correct conidx, key_type, and passkey
 */
void test_key_req_cb_params_passed_correctly(void)
{
    lisa_ble_register_key_req_cb(test_key_req_cb);

    lisa_ble_notify_key_req(2, 1, 654321);

    TEST_ASSERT_EQUAL_INT(1, g_cb_called);
    TEST_ASSERT_EQUAL_UINT8(2, g_key_req_conidx);
    TEST_ASSERT_EQUAL_UINT8(1, g_key_req_type);
    TEST_ASSERT_EQUAL_UINT32(654321, g_key_req_passkey);
}

/**
 * @brief Key request callback receives zero passkey
 */
void test_key_req_cb_zero_passkey(void)
{
    lisa_ble_register_key_req_cb(test_key_req_cb);

    lisa_ble_notify_key_req(0, 0, 0);

    TEST_ASSERT_EQUAL_INT(1, g_cb_called);
    TEST_ASSERT_EQUAL_UINT32(0, g_key_req_passkey);
}

/**
 * @brief Key request callback can be unregistered by passing NULL
 */
void test_key_req_cb_can_be_unregistered(void)
{
    lisa_ble_register_key_req_cb(test_key_req_cb);
    lisa_ble_register_key_req_cb(NULL);

    lisa_ble_notify_key_req(0, 0, 123456);

    TEST_ASSERT_EQUAL_INT(0, g_cb_called);
}

/* ========================================================================
 * Cross-callback independence tests
 * ======================================================================== */

/**
 * @brief conn and disc callbacks are independent — registering one doesn't affect the other
 */
void test_conn_and_disc_callbacks_independent(void)
{
    lisa_ble_register_conn_cb(test_conn_cb);
    /* disc_cb remains NULL */

    gap_bdaddr_t addr = { .addr = {0x11}, .addr_type = 0 };
    lisa_ble_notify_connected(0, 0x0001, &addr);
    lisa_ble_notify_disconnected(0, 0x0001, 0x0013);

    /* Only conn_cb should have fired */
    TEST_ASSERT_EQUAL_INT(1, g_cb_called);
}

/**
 * @brief Full connection lifecycle: connect then disconnect
 */
void test_full_connection_lifecycle(void)
{
    lisa_ble_register_conn_cb(test_conn_cb);
    lisa_ble_register_disc_cb(test_disc_cb);

    gap_bdaddr_t addr = { .addr = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF}, .addr_type = 0 };
    lisa_ble_notify_connected(0, 0x0001, &addr);

    TEST_ASSERT_EQUAL_INT(1, g_cb_called);
    TEST_ASSERT_EQUAL_UINT8(0, g_conn_conidx);

    lisa_ble_notify_disconnected(0, 0x0001, 0x0013);

    TEST_ASSERT_EQUAL_INT(2, g_cb_called);
    TEST_ASSERT_EQUAL_UINT8(0, g_disc_conidx);
    TEST_ASSERT_EQUAL_HEX16(0x0013, g_disc_reason);
}

/* ---- setUp / tearDown ---- */

void setUp(void)
{
    g_cb_called = 0;
    g_cb_status = 0xFFFF;
    s_enable_cmp_cb = NULL;
    s_ble_conn_cb = NULL;
    s_ble_disc_cb = NULL;
    s_ble_bond_cb = NULL;
    s_ble_key_req_cb = NULL;

    memset(&g_conn_peer_addr, 0, sizeof(g_conn_peer_addr));
    g_conn_conidx = 0xFF;
    g_conn_conhdl = 0xFFFF;
    g_disc_conidx = 0xFF;
    g_disc_conhdl = 0xFFFF;
    g_disc_reason = 0;

    g_bond_conidx = 0xFF;
    g_bond_info = 0xFF;
    g_bond_value = 0xFF;
    g_key_req_conidx = 0xFF;
    g_key_req_type = 0xFF;
    g_key_req_passkey = 0;
}

void tearDown(void)
{
}

/*=======MAIN=====*/
int main(void)
{
    UnityBegin("test/components/lisa_bluetooth/test_main.c");

    /* enable_cmp callback tests */
    RUN_TEST(test_init_with_null_callback, __LINE__);
    RUN_TEST(test_init_callback_invoked_on_success, __LINE__);
    RUN_TEST(test_init_callback_invoked_on_failure, __LINE__);
    RUN_TEST(test_callback_invoked_multiple_times, __LINE__);
    RUN_TEST(test_reinit_replaces_callback, __LINE__);
    RUN_TEST(test_init_returns_zero, __LINE__);

    /* BLE connection callback tests */
    RUN_TEST(test_conn_cb_null_no_crash, __LINE__);
    RUN_TEST(test_conn_cb_params_passed_correctly, __LINE__);
    RUN_TEST(test_conn_cb_can_be_replaced, __LINE__);
    RUN_TEST(test_conn_cb_multiple_connections, __LINE__);

    /* BLE disconnection callback tests */
    RUN_TEST(test_disc_cb_null_no_crash, __LINE__);
    RUN_TEST(test_disc_cb_params_passed_correctly, __LINE__);
    RUN_TEST(test_disc_cb_various_reasons, __LINE__);
    RUN_TEST(test_disc_cb_can_be_unregistered, __LINE__);

    /* BLE bond callback tests */
    RUN_TEST(test_bond_cb_null_no_crash, __LINE__);
    RUN_TEST(test_bond_cb_params_passed_correctly, __LINE__);
    RUN_TEST(test_bond_cb_pairing_failed, __LINE__);
    RUN_TEST(test_bond_cb_link_encrypted, __LINE__);
    RUN_TEST(test_bond_cb_can_be_unregistered, __LINE__);

    /* BLE key request callback tests */
    RUN_TEST(test_key_req_cb_null_no_crash, __LINE__);
    RUN_TEST(test_key_req_cb_params_passed_correctly, __LINE__);
    RUN_TEST(test_key_req_cb_zero_passkey, __LINE__);
    RUN_TEST(test_key_req_cb_can_be_unregistered, __LINE__);

    /* Cross-callback tests */
    RUN_TEST(test_conn_and_disc_callbacks_independent, __LINE__);
    RUN_TEST(test_full_connection_lifecycle, __LINE__);

    return (UnityEnd());
}
