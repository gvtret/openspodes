# OpenSPODES

[![CI](https://github.com/gvtret/openspodes/actions/workflows/ci.yml/badge.svg)](https://github.com/gvtret/openspodes/actions/workflows/ci.yml)
[![Docs](https://img.shields.io/badge/docs-GitHub%20Pages-blue)](https://gvtret.github.io/openspodes/)
[![API](https://img.shields.io/badge/API-Doxygen-green)](https://gvtret.github.io/openspodes/api/)

Portable **C11** implementation of **IEC 62056 DLMS/COSEM** (LN referencing), modeled after [spodes-rs](https://github.com/gvtret/spodes-rs) and aligned with the **SPODES / GOST** profile (R 1323565.1).

Designed for embedded and server use: **no heap allocation in the core library**, HAL crypto via function pointers, static buffers.

**Version:** 1.8.0
**License:** GPL-3.0-or-later (see [LICENSE](LICENSE))

## Documentation

| Document | Description |
|----------|-------------|
| [API Reference](https://gvtret.github.io/openspodes/api/) | Doxygen-generated API documentation |
| [Architecture](docs/ARCHITECTURE.md) | Full architecture description, module breakdown, data flows |
| [Security Guide](docs/SECURITY.md) | HLS handshake, suites, keys, glo-ciphering, production checklist |
| [HAL Porting Guide](docs/HAL.md) | MCU porting: transport, crypto, random, timer, mutex, examples |
| [Troubleshooting](docs/TROUBLESHOOTING.md) | Common issues and solutions for HDLC, auth, encryption |

## Features

| Area | Status |
|------|--------|
| Codec (BER/AXDR, compact-array) | ✅ |
| Transport (HDLC session + wrapper) | ✅ |
| HDLC session: SNRM/UA + XID + N(S)/N(R) + DISC/DM + REJ retransmission | ✅ |
| Client / Server session drivers | ✅ |
| GET / SET / ACTION (+ with-list, block transfer) | ✅ |
| General block transfer (unconfirmed + confirmed + lost-block recovery) | ✅ |
| glo- / ded-ciphering (AES-GCM + Kuznyechik suite 8/9) | ✅ |
| HLS mechanisms 0–10 (GMAC, MD5/SHA1/SHA256, GOST CMAC/Sig) | ✅ |
| General ciphering / general-signing | ✅ |
| Push + event notifications | ✅ |
| 42 COSEM IC classes (all Set functional) | ✅ |
| GOST (Streebog, Kuznyechik, GOST 34.10, VKO/KDF) | ✅ |
| Thread safety via HAL mutex | ✅ |
| Linux HAL (TCP, OpenSSL, timer, random) | ✅ |

## Build

Requirements: **CMake ≥ 3.16**, C11 compiler, **OpenSSL** (optional, enables AES-GCM/ECDSA tests and examples).

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

ASAN + UBSan:

```bash
cmake -S . -B build-san -DCMAKE_BUILD_TYPE=Debug -DENABLE_ASAN=ON
cmake --build build-san -j$(nproc)
UBSAN_OPTIONS=print_stacktrace=1 ctest --test-dir build-san --output-on-failure
```

Coverage:

```bash
cmake -S . -B build-cov -DENABLE_COVERAGE=ON
cmake --build build-cov && ctest --test-dir build-cov
python3 scripts/coverage_report.py
```

## Quick start

### 1. Loopback demo (no network)

Full client↔server demo running in-process — zero configuration needed:

```bash
./build/openspodes_loopback_cli demo
```

This performs a complete DLMS/COSEM session:
1. Server registers 2 Data IC objects (OBIS `0.0.1.0.0.255` = 42, `1.0.1.8.0.255` = 123456)
2. Client connects (AARQ → AARE)
3. GET attribute 1 from both objects
4. SET attribute 1 to 999, verify with GET
5. Disconnect (RLRQ → RLRE)

CLI usage for individual operations:
```bash
./build/openspodes_loopback_cli get 1 0.0.1.0.0.255 1    # GET Data OBIS 0.0.1.0.0.255 attr 1
./build/openspodes_loopback_cli set 1 0.0.1.0.0.255 1 u32:100  # SET to 100
```

### 2. Push notifications demo

Unsolicited Data/Event Notification send and receive:

```bash
./build/openspodes_push_listener
```

Demonstrates:
- Server sends Data Notification (0x0F) → client receives and decodes
- Server sends Event Notification (0xC2) → client receives and decodes
- Burst of 5 rapid notifications to verify throughput

### 3. TCP example (port 4059)

```bash
./build/openspodes_tcp_server &
./build/openspodes_tcp_client
```

### 4. Serial/HDLC example

```bash
./build/openspodes_serial_server /dev/ttyUSB0 9600 &
./build/openspodes_serial_client /dev/ttyUSB0 9600
```

### 5. Linux HAL demo

```bash
./build/openspodes_tcp_server &
./build/openspodes_linux_demo 127.0.0.1:4059
```

---

## Usage examples

### Client: GET/SET with security

```c
#include "openspodes.h"
#include "client/client.h"
#include "security/security.h"

/* 1. Initialize transport (TCP, serial, etc.) */
osp_transport_t transport = { .send = my_send, .recv = my_recv, .ctx = ... };

/* 2. Initialize client */
osp_client_t client;
osp_client_init(&client, &transport, OSP_FRAMING_WRAPPER);

/* 3. Configure security (HLS-GMAC, suite 0) */
osp_sec_context_t sec;
osp_sec_context_init(&sec, OSP_SUITE_0, OSP_MECH_HLS_GMAC, system_title);
memcpy(sec.guek, encryption_key, 16);
memcpy(sec.gak, authentication_key, 16);
osp_client_set_security(&client, &sec);

/* 4. Enable block transfer for large payloads */
osp_client_enable_gbt(&client, 64);

/* 5. Connect (AARQ → AARE → HLS pass 3/4) */
osp_err_t r = osp_client_connect(&client, 5000);
if (r != OSP_OK) { /* handle error */ }

/* 6. GET attribute 1 from Data IC (OBIS 0.0.1.8.0.255) */
osp_value_t result;
osp_obis_t obis = {0, 0, 1, 8, 0, 255};
r = osp_client_get(&client, 1, &obis, 1, &result);
if (r == OSP_OK && result.tag == OSP_TAG_DOUBLE_LONG_UNS) {
    printf("Energy: %u kWh\n", result.as.uint32.value);
}

/* 7. SET attribute 1 to 100 */
osp_value_t new_val = osp_val_u32(100);
r = osp_client_set(&client, 1, &obis, 1, &new_val);

/* 8. Release and disconnect */
osp_client_release(&client);
osp_client_disconnect(&client);
```

### Server: register IC objects and accept requests

```c
#include "openspodes.h"
#include "server/server.h"
#include "ic/data.h"
#include "ic/register.h"
#include "ic/clock.h"

/* 1. Initialize transport */
osp_transport_t transport = { .send = my_send, .recv = my_recv, .ctx = ... };

/* 2. Initialize server */
osp_server_t server;
osp_server_init(&server, &transport, OSP_FRAMING_WRAPPER);

/* 3. Configure security */
osp_sec_context_t sec;
osp_sec_context_init(&sec, OSP_SUITE_0, OSP_MECH_LOWEST, system_title);
osp_server_set_security(&server, &sec);

/* 4. Register IC objects */
osp_ic_data_t energy;
osp_ic_data_init(&energy, (osp_obis_t){0, 0, 1, 8, 0, 255});
energy.value = osp_val_u32(123456);
osp_server_register(&server, osp_ic_data_class(), &energy);

osp_ic_register_t voltage;
osp_ic_register_init(&voltage, (osp_obis_t){0, 0, 1, 8, 1, 255});
voltage.value = osp_val_u32(230);
osp_server_register(&server, osp_ic_register_class(), &voltage);

osp_ic_clock_t clock;
osp_ic_clock_init(&clock, (osp_obis_t){0, 0, 1, 0, 0, 255});
osp_server_register(&server, osp_ic_clock_class(), &clock);

/* 5. Accept loop */
for (;;) {
    osp_err_t r = osp_server_accept(&server, 30000);
    if (r == OSP_ERR_TIMEOUT) continue;
    if (r != OSP_OK) break;
}
```

### Server: send push notification

```c
#include "service/notification.h"

/* Send unsolicited Data Notification */
osp_data_notification_t dn;
memset(&dn, 0, sizeof(dn));
dn.long_invoke_id_and_priority = 0x12345678;
dn.date_time_len = 12;
memcpy(dn.date_time, current_time, 12);
dn.notification_body = osp_val_u32(energy_value);

osp_err_t r = osp_server_send_data_notification(&server, &dn);
```

### HDLC session (serial transport)

```c
#include "transport/hdlc_session.h"

/* Initialize HDLC session (client side) */
osp_hdlc_session_t session;
osp_hdlc_session_init_client(&session, &transport,
    0x41, 1,   /* client address: 0x41, 1 byte */
    0x42, 1);  /* server address: 0x42, 1 byte */

/* Connect: SNRM → UA with XID negotiation */
osp_err_t r = osp_hdlc_session_connect(&session, 5000);

/* Send APDU (prepending LLC header + I-frame) */
osp_hdlc_session_send_apdu(&session, apdu, apdu_len);

/* Receive APDU (strips LLC header, updates N(R)) */
uint8_t buf[512];
uint32_t len;
r = osp_hdlc_session_recv_apdu(&session, buf, sizeof(buf), &len, 5000);

/* Disconnect: DISC → UA */
osp_hdlc_session_disconnect(&session, 2000);
```

### Glo-ciphering (encrypted APDU)

```c
#include "security/security.h"

/* Protect: plaintext → encrypted APDU */
osp_sec_context_t sec;
osp_sec_context_init(&sec, OSP_SUITE_0, OSP_MECH_HLS_GMAC, system_title);
memcpy(sec.guek, encryption_key, 16);
memcpy(sec.gak, authentication_key, 16);

uint8_t plaintext[] = {0xC0, 0x01, 0xC1, 0x00};  /* GET request */
uint8_t ciphered[256];
uint32_t ciphered_len;

r = osp_glo_protect(&sec, OSP_GLO_GET_REQUEST,
                     plaintext, sizeof(plaintext),
                     ciphered, &ciphered_len);

/* Unprotect: encrypted → plaintext */
uint8_t recovered[256];
uint32_t recovered_len;
r = osp_glo_unprotect(&sec, ciphered, ciphered_len,
                       recovered, &recovered_len);
```

### Custom HAL transport (bare metal UART)

```c
#include "openspodes.h"

static osp_err_t uart_send(void *ctx, const uint8_t *data, uint32_t len) {
    uart_handle_t *uart = (uart_handle_t *)ctx;
    for (uint32_t i = 0; i < len; i++) {
        uart_putc(uart, data[i]);
    }
    return OSP_OK;
}

static osp_err_t uart_recv(void *ctx, uint8_t *buf, uint32_t size,
                           uint32_t *out_len, uint32_t timeout_ms) {
    uart_handle_t *uart = (uart_handle_t *)ctx;
    uint32_t start = systick_ms();
    uint32_t got = 0;
    while (got < size && (systick_ms() - start) < timeout_ms) {
        if (uart_data_available(uart)) {
            buf[got++] = uart_getc(uart);
        }
    }
    *out_len = got;
    return (got > 0) ? OSP_OK : OSP_ERR_TIMEOUT;
}

/* Use in client/server */
osp_transport_t transport = {
    .send = uart_send,
    .recv = uart_recv,
    .ctx = &my_uart,
};
```

---

## FAQ

### What is OpenSPODES?

OpenSPODES is a portable C11 implementation of the IEC 62056 DLMS/COSEM protocol stack for smart metering. It's designed for embedded systems (MCU) and servers with zero heap allocation in the core library.

### How is this different from other DLMS implementations?

- **Zero heap allocation** — all buffers are static, safe for bare-metal MCU
- **HAL abstraction** — pluggable transport, crypto, timer — port to any platform
- **Full GOST support** — Kuznyechik, Streebog, GOST 34.10 for SPODES deployments
- **Thread-safe** — optional HAL mutex for multi-threaded use
- **42 COSEM IC classes** — all with working Set operations

### Can I use this on a bare-metal MCU?

Yes. The library uses no `malloc`/`free`, no OS primitives, and no threads. All HAL interfaces are optional function pointers. See [docs/HAL.md](docs/HAL.md) for porting guide.

### What platforms are supported?

Any platform with a C11 compiler:
- Bare metal MCU (ARM Cortex-M, RISC-V, etc.)
- FreeRTOS, Zephyr, RT-Thread
- Linux, macOS, Windows (via MinGW)
- Any RTOS with thread/mutex primitives

### How do I enable encryption?

```c
/* Set up security context with keys */
osp_sec_context_t sec;
osp_sec_context_init(&sec, OSP_SUITE_0, OSP_MECH_HLS_GMAC, system_title);
memcpy(sec.guek, encryption_key, 16);  /* Global Unicast Encryption Key */
memcpy(sec.gak, authentication_key, 16);  /* Global Authentication Key */
osp_client_set_security(&client, &sec);
```

See [docs/SECURITY.md](docs/SECURITY.md) for all suites and key management.

### How do I implement my own transport?

Implement the `osp_transport_t` interface:

```c
osp_transport_t transport = {
    .open = my_open,
    .send = my_send,
    .recv = my_recv,
    .close = my_close,
    .is_connected = my_is_connected,
    .ctx = &my_context,
};
```

See [docs/HAL.md](docs/HAL.md) for examples (TCP, UART, SPI).

### How do I use GOST crypto (suite 8/9)?

1. Set suite to `OSP_SUITE_8` or `OSP_SUITE_9`
2. Provide Kuznyechik keys (GUEK, GAK)
3. The library includes built-in Kuznyechik and Streebog implementations
4. For production, use certified crypto modules via HAL

### How do I register custom IC objects?

```c
/* Implement the IC vtable */
static osp_err_t my_get(const void *inst, uint8_t attr_id, osp_value_t *result) {
    /* Return attribute value */
}

static osp_err_t my_set(void *inst, uint8_t attr_id, const osp_value_t *value) {
    /* Store attribute value */
}

static const osp_ic_class_t my_ic = {
    .name = "My Custom IC",
    .class_id = 9999,
    .get_attr = my_get,
    .set_attr = my_set,
};

/* Register with server */
osp_server_register(&server, &my_ic, &my_instance);
```

### How do I send push notifications?

```c
/* Data Notification (unsolicited) */
osp_data_notification_t dn;
memset(&dn, 0, sizeof(dn));
dn.long_invoke_id_and_priority = 0x12345678;
dn.notification_body = osp_val_u32(energy_value);
osp_server_send_data_notification(&server, &dn);

/* Event Notification */
osp_event_notification_t ev;
memset(&ev, 0, sizeof(ev));
ev.has_time = 1;
memcpy(ev.time, current_time, 12);
ev.attribute.class_id = 1;
ev.attribute.instance_id = (osp_obis_t){0, 0, 1, 8, 0, 255};
ev.attribute.attribute_id = 2;
ev.value = osp_val_u32(42);
osp_server_send_event_notification(&server, &ev);
```

### What is the maximum APDU size?

Default is 1024 bytes (`OSP_CLIENT_MAX_PDU`, `OSP_SERVER_MAX_PDU`). Increase for larger payloads:

```c
#define OSP_CLIENT_MAX_PDU 2048
#define OSP_SERVER_MAX_PDU 2048
```

### How do I handle block transfer for large payloads?

Enable GBT on the client:

```c
osp_client_enable_gbt(&client, 64);  /* block size = 64 bytes */
osp_client_set_gbt_window(&client, 4);  /* window size = 4 */
```

The library automatically fragments large APDUs and handles reassembly.

### How do I debug HDLC issues?

1. Check baud rate and physical connection
2. Verify HDLC addresses match between client/server
3. Enable raw frame logging in your transport implementation
4. Try the loopback example first (no network issues)

See [docs/TROUBLESHOOTING.md](docs/TROUBLESHOOTING.md) for more.

### Is this production-ready?

Yes. The library:
- Passes ASAN + UBSan checks
- Has 16 test suites with 300+ test functions
- Implements all HLS mechanisms 0-10
- Has proper key zeroization (`osp_sec_context_destroy`)
- Handles invocation counter overflow
- Supports REJ retransmission

See [docs/SECURITY.md](docs/SECURITY.md) for production security checklist.

### How do I contribute?

1. Fork the repository
2. Create a feature branch
3. Ensure all tests pass: `ctest --test-dir build`
4. Ensure ASAN passes: `ctest --test-dir build-san`
5. Submit a pull request

---

## HAL interfaces

| Interface | Purpose |
|-----------|---------|
| `osp_transport_t` | TCP/serial send/recv |
| `osp_hal_gcm_crypt` | AES-128-GCM encrypt/decrypt |
| `osp_hal_md5/sha1/sha256` | Hash functions for HLS |
| `osp_hal_random_fill` | Cryptographic random bytes |
| `osp_timer_t` | Timing (now_ms, delay_ms) |
| `osp_system_t` | System title + key store |
| `osp_mutex_t` | Optional thread-safety (bare-metal: NULL) |

See [docs/HAL.md](docs/HAL.md) for MCU porting guide with examples for bare metal, FreeRTOS, Zephyr, and Linux/pthreads.

See [examples/linux_hal.h](examples/linux_hal.h) for complete Linux HAL implementation.

## Security

- Set **unique** `system_title`, keys, and monotonic **invocation counters** per association
- Use **HLS** (not LLS) on untrusted links; prefer suite 1/2 or GOST suite 8/9
- **Dedicated keys**: transfer only inside authenticated+encrypted `glo-initiate-request`
- Replay protection: `osp_glo_unprotect` rejects non-increasing IC
- Wire real crypto HAL in production — bundled GOST is for portability/testing

See [docs/SECURITY.md](docs/SECURITY.md) for full security guide.

## Layout

```
src/codec/       BER/AXDR, COSEM serialize
src/transport/   HDLC session + wrapper
src/service/     xDLMS APDUs, ACSE, GBT, initiate
src/security/    HLS, glo/ded, GOST (Streebog, Kuznyechik, GOST 34.10)
src/client/      Session client
src/server/      Dispatcher + accept loop
src/ic/          42 interface classes
src/spodus/      SPODUS concentrator runtime
tests/           CMocka suites (16 CTest targets, 300+ test functions)
examples/        loopback, push listener, serial, TCP, Linux HAL demo
docs/            Architecture, Security, HAL porting, Troubleshooting
```

## References

- [IEC 62056-5-3](https://webstore.iec.ch/en/publication/26676) (xDLMS)
- [IEC 62056-46](https://webstore.iec.ch/en/publication/26672) (HDLC)
- [IEC 62056-47](https://webstore.iec.ch/en/publication/26673) (wrapper)
- [ISO/IEC 13239](https://webstore.iec.ch/en/publication/8872) (HDLC frame format)
- [R 1323565.1](https://docs.cntd.ru/document/1200139859) (GOST transport & HLS)
- [spodes-rs](https://github.com/gvtret/spodes-rs) — Rust reference implementation

## License

GPL-3.0-or-later. See [LICENSE](LICENSE).

This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

## Contributing

Contributions are welcome! Please:

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/my-feature`)
3. Ensure all tests pass (`ctest --test-dir build`)
4. Ensure ASAN passes (`ctest --test-dir build-san`)
5. Submit a pull request

See [CONTRIBUTING.md](CONTRIBUTING.md) for detailed guidelines.

## Support

- **Issues**: [GitHub Issues](https://github.com/gvtret/openspodes/issues)
- **Discussions**: [GitHub Discussions](https://github.com/gvtret/openspodes/discussions)
- **Documentation**: [GitHub Pages](https://gvtret.github.io/openspodes/)
- **Security vulnerabilities**: Report privately via [security advisories](https://github.com/gvtret/openspodes/security/advisories)

## Authors

- **gvtret** — [GitHub](https://github.com/gvtret)

## Acknowledgments

- [spodes-rs](https://github.com/gvtret/spodes-rs) — Rust reference implementation that inspired this project
- [dlms-codec](https://github.com/Gurux/gurux.dlms) — Reference DLMS codec for test vectors
- IEC 62056 standard series — DLMS/COSEM protocol specifications
