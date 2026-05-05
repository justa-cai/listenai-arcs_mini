#!/usr/bin/env python3
"""Serial port reader using Python stdlib only (no pyserial dependency).

sdk-assistant-agent 自带，用于 Agent 自动抓取串口日志闭环。
- Works in non-TTY environments (e.g., subprocess, CI pipelines)
- DTR/RTS signal control via ioctl
- Non-blocking read with timeout via select()
"""

import argparse
import fcntl
import os
import select
import struct
import sys
import termios
import time

# ioctl constants for modem line control
TIOCMBIS = 0x5416  # Set modem bits
TIOCMBIC = 0x5417  # Clear modem bits
TIOCM_DTR = 0x002
TIOCM_RTS = 0x004

BAUD_MAP = {
    9600: termios.B9600,
    19200: termios.B19200,
    38400: termios.B38400,
    57600: termios.B57600,
    115200: termios.B115200,
    230400: termios.B230400,
    460800: termios.B460800,
    921600: termios.B921600,
    1000000: termios.B1000000,
    2000000: termios.B2000000,
    3000000: termios.B3000000,
}


def _set_modem_signal(fd, signal_bit, assert_high):
    """Assert or deassert a modem control signal (DTR/RTS) via ioctl."""
    ioctl_op = TIOCMBIS if assert_high else TIOCMBIC
    fcntl.ioctl(fd, ioctl_op, struct.pack("I", signal_bit))


def serial_read(port, baudrate=921600, timeout=5, dtr=False, rts=False):
    """Open serial port, configure it, and read data until timeout.

    Args:
        port: Device path (e.g., /dev/ttyACM0)
        baudrate: Baud rate (default: 921600)
        timeout: Read timeout in seconds (default: 5)
        dtr: If True, assert DTR high; if False, pull DTR low (default: False)
        rts: If True, assert RTS high; if False, pull RTS low (default: False)

    Returns:
        Decoded string of all received data.
    """
    baud = BAUD_MAP.get(baudrate)
    if baud is None:
        supported = ", ".join(str(b) for b in sorted(BAUD_MAP.keys()))
        raise ValueError(f"Unsupported baudrate {baudrate}. Supported: {supported}")

    fd = os.open(port, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
    try:
        # Configure raw mode with termios
        attrs = termios.tcgetattr(fd)
        # Input flags: disable all processing
        attrs[0] = 0
        # Output flags: disable all processing
        attrs[1] = 0
        # Control flags: 8N1, enable receiver, ignore modem control lines
        attrs[2] = termios.CS8 | termios.CREAD | termios.CLOCAL
        # Local flags: disable all (raw mode)
        attrs[3] = 0
        # Input speed
        attrs[4] = baud
        # Output speed
        attrs[5] = baud
        # Control characters
        attrs[6][termios.VMIN] = 0
        attrs[6][termios.VTIME] = 0
        termios.tcsetattr(fd, termios.TCSANOW, attrs)

        # Flush any stale data in input/output buffers
        termios.tcflush(fd, termios.TCIOFLUSH)

        _set_modem_signal(fd, TIOCM_DTR, dtr)
        _set_modem_signal(fd, TIOCM_RTS, rts)

        # Read loop with select-based timeout
        end_time = time.monotonic() + timeout
        output = bytearray()

        while True:
            remaining = end_time - time.monotonic()
            if remaining <= 0:
                break

            readable, _, _ = select.select([fd], [], [], min(remaining, 0.5))
            if readable:
                try:
                    data = os.read(fd, 4096)
                    if data:
                        output.extend(data)
                        # Write to stdout immediately for real-time output
                        sys.stdout.buffer.write(data)
                        sys.stdout.buffer.flush()
                except OSError:
                    break

        return output.decode("utf-8", errors="replace")
    finally:
        os.close(fd)


def main():
    parser = argparse.ArgumentParser(
        description="Read serial port output (Python stdlib, no pyserial needed)"
    )
    parser.add_argument("port", help="Serial device path (e.g., /dev/ttyACM0)")
    parser.add_argument(
        "-b", "--baudrate", type=int, default=921600, help="Baud rate (default: 921600)"
    )
    parser.add_argument(
        "-t", "--timeout", type=float, default=5, help="Read timeout in seconds (default: 5)"
    )
    parser.add_argument(
        "--dtr", action="store_true", help="Assert DTR high (default: pull low)"
    )
    parser.add_argument(
        "--rts", action="store_true", help="Assert RTS high (default: pull low)"
    )

    args = parser.parse_args()

    try:
        serial_read(
            port=args.port,
            baudrate=args.baudrate,
            timeout=args.timeout,
            dtr=args.dtr,
            rts=args.rts,
        )
    except PermissionError:
        print(f"Error: Permission denied for {args.port}", file=sys.stderr)
        print("Hint: ensure user is in uucp/dialout group", file=sys.stderr)
        sys.exit(1)
    except FileNotFoundError:
        print(f"Error: Device not found: {args.port}", file=sys.stderr)
        sys.exit(1)
    except (ValueError, OSError) as e:
        print(f"Error: {e}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
