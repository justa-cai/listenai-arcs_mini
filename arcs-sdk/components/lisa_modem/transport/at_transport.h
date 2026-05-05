/**
 * @file at_transport.h
 * @brief AT Command Transport Abstraction Layer
 * @details Defines the transport interface for AT command communication.
 *          Implementations can be UART, SPI, USB, etc.
 */

#ifndef __AT_TRANSPORT_H__
#define __AT_TRANSPORT_H__

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Transport ioctl commands */
#define AT_TRANSPORT_IOCTL_SET_BAUDRATE     0x01
#define AT_TRANSPORT_IOCTL_GET_BAUDRATE     0x02
#define AT_TRANSPORT_IOCTL_SET_FLOW_CTRL    0x03
#define AT_TRANSPORT_IOCTL_RX_ENABLE        0x04
#define AT_TRANSPORT_IOCTL_RX_DISABLE       0x05

typedef struct at_transport at_transport_t;

/**
 * @brief Transport receive callback
 *
 * Called by transport implementation when data is received.
 *
 * @param data Received data buffer
 * @param len Data length
 * @param ctx User context
 */
typedef void (*at_transport_rx_cb_t)(const uint8_t *data, size_t len, void *ctx);

/**
 * @brief Transport operations interface
 */
typedef struct {
    /**
     * @brief Open/initialize the transport
     * @param transport Transport instance
     * @return 0 on success, negative on error
     */
    int (*open_fn)(at_transport_t *transport);

    /**
     * @brief Close/deinitialize the transport
     * @param transport Transport instance
     */
    void (*close_fn)(at_transport_t *transport);

    /**
     * @brief Send data through the transport
     * @param transport Transport instance
     * @param data Data to send
     * @param len Data length
     * @return Number of bytes sent, or negative on error
     */
    int (*send_fn)(at_transport_t *transport, const uint8_t *data, size_t len);

    /**
     * @brief Read data from the transport
     * @param transport Transport instance
     * @param data Output buffer
     * @param len Buffer size
     * @param timeout_ms Read timeout in milliseconds
     * @return >0 bytes read, 0 on timeout/no data, negative on error
     */
    int (*read_fn)(at_transport_t *transport, uint8_t *data, size_t len, uint32_t timeout_ms);

    /**
     * @brief Set receive callback
     * @param transport Transport instance
     * @param cb Callback function (NULL to unregister)
     * @param ctx User context passed to callback
     * @return 0 on success, negative on error
     */
    int (*set_rx_callback_fn)(at_transport_t *transport, at_transport_rx_cb_t cb, void *ctx);

    /**
     * @brief Transport-specific control
     * @param transport Transport instance
     * @param cmd Control command (AT_TRANSPORT_IOCTL_*)
     * @param arg Command-specific argument
     * @return 0 on success, negative on error
     */
    int (*ioctl_fn)(at_transport_t *transport, int cmd, void *arg);

    /**
     * @brief Destroy the transport instance
     * @param transport Transport instance
     */
    void (*destroy_fn)(at_transport_t *transport);
} at_transport_ops_t;

/**
 * @brief Transport instance
 */
struct at_transport {
    const at_transport_ops_t *ops;   /**< Transport operations */
    void *priv;                      /**< Implementation private data */
};

/* Inline helper functions */

static inline int at_transport_open(at_transport_t *transport)
{
    if (!transport || !transport->ops || !transport->ops->open_fn) return -1;
    return transport->ops->open_fn(transport);
}

static inline void at_transport_close(at_transport_t *transport)
{
    if (transport && transport->ops && transport->ops->close_fn) {
        transport->ops->close_fn(transport);
    }
}

static inline int at_transport_send(at_transport_t *transport, const uint8_t *data, size_t len)
{
    if (!transport || !transport->ops || !transport->ops->send_fn) return -1;
    return transport->ops->send_fn(transport, data, len);
}

static inline int at_transport_read(at_transport_t *transport,
                                    uint8_t *data, size_t len, uint32_t timeout_ms)
{
    if (!transport || !transport->ops || !transport->ops->read_fn) return -1;
    return transport->ops->read_fn(transport, data, len, timeout_ms);
}

static inline int at_transport_set_rx_callback(at_transport_t *transport,
                                                at_transport_rx_cb_t cb, void *ctx)
{
    if (!transport || !transport->ops || !transport->ops->set_rx_callback_fn) return -1;
    return transport->ops->set_rx_callback_fn(transport, cb, ctx);
}

static inline int at_transport_ioctl(at_transport_t *transport, int cmd, void *arg)
{
    if (!transport || !transport->ops || !transport->ops->ioctl_fn) return -1;
    return transport->ops->ioctl_fn(transport, cmd, arg);
}

static inline void at_transport_destroy(at_transport_t *transport)
{
    if (transport && transport->ops && transport->ops->destroy_fn) {
        transport->ops->destroy_fn(transport);
    }
}

#ifdef __cplusplus
}
#endif

#endif /* __AT_TRANSPORT_H__ */
