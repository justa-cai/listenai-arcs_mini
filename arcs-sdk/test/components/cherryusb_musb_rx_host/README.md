# ARCS MUSB receive and control endpoint regression

Runs the actual MUSB DCD against in-memory register storage. A FIFO with
`RXRDY=1` and `RXIS=0` represents a packet whose interrupt was already read and
cleared while the endpoint was masked. Checks delayed rearming, ordinary IRQ
reception, simultaneous RX/SOF notification, and bus reset. Both one-shot SOF
and an application's permanently enabled SOF subscription are covered.
Callbacks must occur only when the test dispatches the ISR, never inline from
`usbd_ep_start_read()`.

From the SDK root:

```sh
cmake -S test/components/cherryusb_musb_rx_host -B build-musb-rx-host \
  -DCMAKE_C_FLAGS='-fsanitize=address,undefined -g'
cmake --build build-musb-rx-host
ctest --test-dir build-musb-rx-host --output-on-failure
```

The register model does not emulate USB transactions or the interrupt controller.
Also validate on `arcs_mini`: repeatedly reboot with the Mac ADB server running,
check that the device returns online, then type multiple commands in `adb shell`
and exit with Ctrl-C. Capture the board UART throughout the test.

EP0 cases inject a new SETUP together with SentStall or SetupEnd. The driver
must return to SETUP processing instead of dropping the request or reporting
a completion for the aborted data phase (MUSB programming guide, section
21.1.5). These conditions can occur during host descriptor probing.

Also covers a new SETUP coalesced with delayed IN data/status completion,
and confirms that an eight-byte control OUT payload is still treated as data.
The former must bypass the old completion: its zero-length status read would
otherwise acknowledge and discard the newly received SETUP.

After building the ADB firmware, verify the actual ELF's device FIFO table:

```sh
python3 test/components/cherryusb_musb_rx_host/check_adb_fifo.py /path/to/build/arcs-mini
```

The check requires independent EP2 IN/OUT buffers that each fit a 512-byte HS
bulk packet, and checks the complete table against the 4 KiB ARCS FIFO RAM.
It catches `FIFO_TXRX` overlap even though ordinary half-duplex traffic may pass.
