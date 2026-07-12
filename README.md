# OpenSPODES

[![CI](https://github.com/gvtret/openspodes/actions/workflows/ci.yml/badge.svg)](https://github.com/gvtret/openspodes/actions/workflows/ci.yml)
[![Docs](https://img.shields.io/badge/docs-GitHub%20Pages-blue)](https://gvtret.github.io/openspodes/)
[![API](https://img.shields.io/badge/API-Doxygen-green)](https://gvtret.github.io/openspodes/api/)

Portable **C11** implementation of **IEC 62056 DLMS/COSEM** (LN referencing), modeled after [spodes-rs](https://github.com/gvtret/spodes-rs) and aligned with the **SPODES / GOST** profile (R 1323565.1).

Designed for embedded and server use: **no heap allocation in the core library**, HAL crypto via function pointers, static buffers.

**Version:** 1.2.0
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

### Loopback (in-process)

Full client↔server demo with no network — runs entirely in-process using mock transport.

```bash
./build/openspodes_loopback_cli demo
```

The demo performs:
1. Initializes server with 2 Data IC objects (OBIS `0.0.1.0.0.255` = 42, `1.0.1.8.0.255` = 123456)
2. Connects client to server (AARQ → AARE, lowest security)
3. GET attribute 1 from both objects
4. SET attribute 1 on the first object to 999
5. Verifies the new value with GET
6. Disconnects (RLRQ → RLRE)

CLI usage:
```bash
./build/openspodes_loopback_cli get 1 0.0.1.0.0.255 1
./build/openspodes_loopback_cli set 1 0.0.1.0.0.255 1 u32:100
```

### Push notifications (in-process)

Demonstrates unsolicited Data/Event Notification send and receive:

```bash
./build/openspodes_push_listener
```

### TCP wrapper example (port 4059)

```bash
./build/openspodes_tcp_server &
./build/openspodes_tcp_client
```

### Serial/HDLC example

```bash
./build/openspodes_serial_server /dev/ttyUSB0 9600 &
./build/openspodes_serial_client /dev/ttyUSB0 9600
```

### Linux HAL demo

```bash
./build/openspodes_tcp_server &
./build/openspodes_linux_demo 127.0.0.1:4059
```

## Integration pattern

### Client

```c
#include "openspodes.h"
#include "client/client.h"

osp_client_t client;
osp_client_init(&client, &transport, OSP_FRAMING_WRAPPER);

osp_sec_context_t sec;
osp_sec_context_init(&sec, OSP_SUITE_0, OSP_MECH_HLS_GMAC, system_title);
osp_client_set_security(&client, &sec);

osp_client_enable_gbt(&client, 64);
osp_client_connect(&client, 5000);

osp_value_t result;
osp_obis_t obis = {1, 0, 1, 8, 0, 255};
osp_client_get(&client, 1, &obis, 1, &result);

osp_client_release(&client);
osp_client_disconnect(&client);
```

### Server

```c
#include "openspodes.h"
#include "server/server.h"
#include "ic/data.h"

osp_server_t server;
osp_server_init(&server, &transport, OSP_FRAMING_WRAPPER);

osp_ic_data_t data_obj;
osp_ic_data_init(&data_obj, (osp_obis_t){1, 0, 1, 8, 0, 255});
data_obj.value = osp_val_u32(123456);
osp_server_register(&server, osp_ic_data_class(), &data_obj);

for (;;) {
    osp_err_t r = osp_server_accept(&server, 30000);
    if (r == OSP_ERR_TIMEOUT) continue;
    if (r != OSP_OK) break;
}
```

### HDLC session

```c
#include "transport/hdlc_session.h"

osp_hdlc_session_t session;
osp_hdlc_session_init_client(&session, &transport, 2, 1, 3, 1);

osp_hdlc_session_connect(&session, 5000);
osp_hdlc_session_send_apdu(&session, apdu, apdu_len);
osp_hdlc_session_recv_apdu(&session, buf, sizeof(buf), &len, 5000);
osp_hdlc_session_disconnect(&session, 2000);
```

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
