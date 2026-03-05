# ML307 4G Module Driver

This directory contains the ML307 4G module driver implementation for ARCS SDK, refactored from the ESP32-based c_version implementation.

## Architecture Overview

```
┌─────────────────────────────────────────────────────────┐
│                   Application Layer                      │
│  (HTTP Client, MQTT Client, Custom Protocols, etc.)     │
└───────────────────────┬─────────────────────────────────┘
                        │
┌───────────────────────▼─────────────────────────────────┐
│              ml307_wrapper.h/c                           │
│   High-level Modem Management & Network Operations      │
│   - Module initialization & reboot                       │
│   - Network registration monitoring (+CREG URC)          │
│   - PDP context management (+MIPCALL URC)               │
│   - Connection factory (TCP/SSL)                         │
└───────────────────────┬─────────────────────────────────┘
                        │
┌───────────────────────▼─────────────────────────────────┐
│         ml307_tcp.h/c  &  ml307_udp.h/c                 │
│     TCP/SSL & UDP Connection Management                  │
│   - Connection lifecycle (connect/disconnect)            │
│   - Data transmission with hex encoding                  │
│   - URC handling (MIPOPEN, MIPCLOSE, MIPSEND, MIPURC)  │
│   - Stream & disconnect callbacks                        │
└───────────────────────┬─────────────────────────────────┘
                        │
┌───────────────────────▼─────────────────────────────────┐
│                at_uart.h/c                               │
│         AT Command Communication Layer                   │
│   - Command/response synchronization (EventGroups)       │
│   - URC (Unsolicited Result Code) parsing               │
│   - Argument parsing (string/int/double)                 │
│   - Dynamic buffer management                            │
└───────────────────────┬─────────────────────────────────┘
                        │
┌───────────────────────▼─────────────────────────────────┐
│              lisa_uart (ARCS SDK)                        │
│         Hardware UART Driver Interface                   │
└─────────────────────────────────────────────────────────┘
```

## File Structure

### Core Implementation Files

| File | Description | Lines |
|------|-------------|-------|
| [at_uart.h](at_uart.h) | AT command interface definitions | ~150 |
| [at_uart.c](at_uart.c) | AT command implementation with URC parsing | ~820 |
| [ml307_tcp.h](ml307_tcp.h) | TCP/SSL connection interface | ~153 |
| [ml307_tcp.c](ml307_tcp.c) | TCP/SSL connection implementation | ~620 |
| [ml307_wrapper.h](ml307_wrapper.h) | High-level modem management interface | ~100 |
| [ml307_wrapper.c](ml307_wrapper.c) | Modem management implementation | ~388 |

### Documentation & Examples

| File | Description |
|------|-------------|
| [README.md](README.md) | This file - architecture overview |
| [at_uart_usage_example.md](at_uart_usage_example.md) | AT UART layer usage guide |
| [ml307_usage_example.md](ml307_usage_example.md) | Complete ML307 usage guide with examples |
| [ml307_example.c](ml307_example.c) | Compilable example code |

### Reference Implementation (ESP32-based)

| File | Description |
|------|-------------|
| [c_version/at_uart.h](c_version/at_uart.h) | Original ESP32 AT interface |
| [c_version/at_uart.c](c_version/at_uart.c) | Original ESP32 AT implementation |
| [c_version/ml307_tcp.c](c_version/ml307_tcp.c) | Original ESP32 TCP implementation |
| [c_version/ml307_at_modem.c](c_version/ml307_at_modem.c) | Original ESP32 modem management |

## Key Features

### AT UART Layer (at_uart.c)

- ✅ **Command/Response Synchronization**: EventGroup-based waiting mechanism
- ✅ **URC Parsing**: Automatic detection and callback dispatch for unsolicited messages
- ✅ **Argument Parsing**: Automatic type detection (string/int/double)
- ✅ **Dynamic Buffers**: Auto-growing receive/response buffers
- ✅ **Ping-Pong Buffers**: Dual circular buffers for continuous reception
- ✅ **Error Handling**: OK/ERROR/+CME ERROR parsing
- ✅ **Thread Safety**: Multiple mutexes for concurrent access
- ✅ **Debug Mode**: Optional AT command logging

### TCP Layer (ml307_tcp.c)

- ✅ **Connection Management**: TCP and SSL/TLS support
- ✅ **Hex Encoding**: Automatic encoding/decoding for binary data
- ✅ **Event-driven**: Connection, disconnection, data callbacks
- ✅ **Multiple Connections**: Support for 6 simultaneous connections (ID 0-5)
- ✅ **Large Data Transfer**: Automatic chunking (730 bytes per packet)
- ✅ **Error Reporting**: Last error code tracking
- ✅ **State Machine**: Proper connection state management

### Wrapper Layer (ml307_wrapper.c)

- ✅ **Auto-initialization**: Complete module setup sequence
- ✅ **Network Monitoring**: Real-time network status via URC
- ✅ **PDP Context**: Automatic IP address detection
- ✅ **Connection Factory**: Simplified TCP/SSL instance creation
- ✅ **Sleep Mode**: Power management support
- ✅ **Connection Reset**: Cleanup utility for all connections
- ✅ **Reboot Support**: Module reset capability

## Quick Start

### 1. Basic HTTP GET Request

```c
#include "ml307_wrapper.h"
#include "ml307_tcp.h"

// Initialize modem
ml307_wrapper_t *modem = ml307_wrapper_create();

// Wait for network
ml307_network_check(modem, 30000);

// Create TCP connection
ml307_tcp_t *tcp = ml307_wrapper_create_tcp(modem, 0);

// Connect to server
ml307_tcp_connect(tcp, "httpbin.org", 80);

// Send HTTP request
const char *request = "GET /get HTTP/1.1\r\nHost: httpbin.org\r\n\r\n";
ml307_tcp_send(tcp, request, strlen(request));

// Cleanup
ml307_tcp_disconnect(tcp);
ml307_tcp_destroy(tcp);
ml307_wrapper_destroy(modem);
```

See [ml307_usage_example.md](ml307_usage_example.md) for comprehensive examples.

### 2. With Data Callbacks

```c
void on_data(const char *data, size_t len, void *user_data) {
    printf("Received: %.*s\n", (int)len, data);
}

void on_disconnect(void *user_data) {
    printf("Connection closed\n");
}

// Register callbacks before connecting
ml307_tcp_on_stream(tcp, on_data, NULL);
ml307_tcp_on_disconnected(tcp, on_disconnect, NULL);
ml307_tcp_connect(tcp, "example.com", 80);
```

## Migration from c_version

### Key Changes

1. **Driver Interface**: ESP32 `Driver_UART.h` → ARCS `lisa_uart.h`
2. **FreeRTOS Headers**: `freertos/xxx.h` → `FreeRTOS.h`
3. **GPIO Interface**: ESP32 GPIO → ARCS GPIO (if used)
4. **Global Instance**: `at_uart_t *uart` parameter → Global singleton pattern
5. **Configuration**: Kconfig-based GPIO pins → Runtime configuration

### API Compatibility

Most APIs remain compatible:

```c
// ESP32 version
at_uart_send_command(uart, "AT", 1000, true);

// ARCS version (global instance)
at_uart_send_command("AT", 1000, true);
```

## Configuration

### UART Configuration (at_uart.c)

```c
#define AT_UART_DEVICE          LISA_UART_0
#define AT_UART_BAUD_RATE       115200
#define AT_UART_RX_BUF_SIZE     1024
#define AT_UART_TX_BUF_SIZE     512
```

### TCP Timeouts (ml307_tcp.h)

```c
#define TCP_CONNECT_TIMEOUT_MS  10000  // 10 seconds
#define TCP_SEND_TIMEOUT_MS     5000   // 5 seconds
```

### Network Wait (ml307_wrapper.c)

```c
// Wait up to 30 seconds for network ready
ml307_network_check(modem, 30000);
```

## Debugging

### Enable AT Command Debug

```c
at_uart_set_debug(true);
```

This will log all AT commands and responses:

```
[AT_TX] AT+CREG?
[AT_RX] +CREG: 2,1,"1234","5678"
[AT_RX] OK
```

### Check Network Status

```c
network_status_t status = ml307_wrapper_get_network_status(modem);
bool ready = ml307_wrapper_is_network_ready(modem);
```

### Get Last Error

```c
int error = ml307_tcp_get_last_error(tcp);
printf("Error code: %d\n", error);
```

## Thread Safety

All modules are thread-safe:

- **at_uart**: Uses `cmd_mutex`, `tx_mutex`, `buffer_mutex`
- **ml307_tcp**: Uses `mutex` for instance data
- **ml307_wrapper**: Uses `event_group` for synchronization

Multiple tasks can safely:
- Send AT commands concurrently (serialized by mutex)
- Create multiple TCP connections (different IDs)
- Register callbacks from different tasks

## Limitations

1. **Maximum Connections**: 6 simultaneous TCP/SSL connections (ID 0-5)
2. **Maximum Packet Size**: 730 bytes per MIPSEND command
3. **Buffer Sizes**: RX buffer max 8192 bytes (configurable)
4. **URC Callbacks**: Executed in UART task context (keep handlers short)
5. **SSL/TLS**: Uses ML307 built-in SSL (limited configuration options)

## Troubleshooting

### Module Not Responding

```c
// Enable debug to see AT traffic
at_uart_set_debug(true);

// Try rebooting module
ml307_wrapper_reboot(modem);
vTaskDelay(pdMS_TO_TICKS(5000));
```

### Network Registration Fails

```c
// Check SIM card
at_uart_send_command("AT+CPIN?", 1000, true);

// Check signal strength
at_uart_send_command("AT+CSQ", 1000, true);

// Check operator
at_uart_send_command("AT+COPS?", 1000, true);
```

### TCP Connection Fails

- Ensure network is ready: `ml307_wrapper_is_network_ready()`
- Check connection ID (0-5)
- Verify server hostname and port
- Increase timeout if on slow network

### Data Not Received

- Register callbacks BEFORE connecting
- Check callback function is being called
- Enable debug mode to see URC messages
- Verify data format (hex encoded)

## Performance

### Typical Timing

- Module initialization: ~3-5 seconds
- Network registration: 5-30 seconds (depends on signal)
- TCP connection: 1-10 seconds
- Data send latency: ~100-500ms
- Data receive latency: ~100-500ms

### Memory Usage

- `at_uart`: ~8KB (buffers + instance)
- `ml307_tcp`: ~2KB per connection
- `ml307_wrapper`: ~1KB
- Total: ~11KB + (2KB × num_connections)

## Testing

See [ml307_example.c](ml307_example.c) for complete test examples:

```c
// Start all examples
ml307_start_examples();
```

This will run:
1. HTTP GET request
2. TCP echo client
3. HTTPS request with SSL
4. Network status monitoring

## References

- **ML307 AT Command Manual**: Official ML307 documentation
- **ARCS SDK Documentation**: lisa_uart driver reference
- **FreeRTOS API**: Task, EventGroup, Mutex APIs
- [at_uart_usage_example.md](at_uart_usage_example.md): Detailed AT layer guide
- [ml307_usage_example.md](ml307_usage_example.md): Complete usage guide

## License

This implementation is part of the ARCS SDK voice assistant project.

## Change Log

### Version 1.0 (2024-12)

- Initial implementation based on c_version
- Migrated from ESP32 to ARCS SDK
- Added comprehensive documentation
- Added example code

---

For detailed API documentation and usage examples, see:
- [at_uart_usage_example.md](at_uart_usage_example.md)
- [ml307_usage_example.md](ml307_usage_example.md)
- [ml307_example.c](ml307_example.c)
