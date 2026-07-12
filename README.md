# OpenSPODES

[![CI](https://github.com/gvtret/openspodes/actions/workflows/ci.yml/badge.svg)](https://github.com/gvtret/openspodes/actions/workflows/ci.yml)

Portable **C11** implementation of **IEC 62056 DLMS/COSEM** (LN referencing), modeled after [spodes-rs](https://github.com/gvtret/spodes-rs) and aligned with the **SPODES / GOST** profile (R 1323565.1).

Designed for embedded and server use: **no heap allocation in the core library**, HAL crypto via function pointers, static buffers.

**Version:** 1.0.0
**License:** GPL-3.0-or-later (see [LICENSE](LICENSE))

## Features

| Area | Status |
|------|--------|
| Codec (BER/AXDR, compact-array) | ✅ |
| Transport (HDLC session + wrapper) | ✅ |
| HDLC session: SNRM/UA + XID + N(S)/N(R) + DISC/DM | ✅ |
| Client / Server session drivers | ✅ |
| GET / SET / ACTION (+ with-list, block transfer) | ✅ |
| General block transfer (unconfirmed + confirmed + lost-block recovery) | ✅ |
| glo- / ded-ciphering (AES-GCM + Kuznyechik suite 8/9) | ✅ |
| HLS (GMAC, MD5/SHA1/SHA256, GOST 8–10) | ✅ |
| general-ciphering / general-signing | ✅ |
| Push + event notifications | ✅ |
| 42 COSEM IC classes | ✅ |
| GOST (Streebog, Kuznyechik, GOST 34.10, VKO/KDF) | ✅ |
| Linux HAL (TCP, OpenSSL, timer, random) | ✅ |

## Build

Requirements: **CMake ≥ 3.16**, C11 compiler, **OpenSSL** (optional, enables AES-GCM/ECDSA tests and examples).

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

Coverage (gcov):

```bash
cmake -S . -B build-cov -DENABLE_COVERAGE=ON
cmake --build build-cov && ctest --test-dir build-cov
python3 scripts/coverage_report.py   # run inside build-cov/
```

## Quick start

### Loopback (in-process)

```bash
./build/openspodes_loopback_cli demo
```

### TCP wrapper example (port 4059)

```bash
./build/openspodes_tcp_server &
./build/openspodes_tcp_client
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

### HDLC session (serial/TCP with HDLC framing)

```c
#include "transport/hdlc_session.h"

osp_hdlc_session_t session;
osp_hdlc_session_init_client(&session, &transport, 2, 1, 3, 1);

osp_hdlc_session_connect(&session, 5000);  /* SNRM/UA + XID */
osp_hdlc_session_send_apdu(&session, apdu, apdu_len);
osp_hdlc_session_recv_apdu(&session, buf, sizeof(buf), &len, 5000);
osp_hdlc_session_disconnect(&session, 2000);  /* DISC/UA */
```

## HAL interfaces

The library provides pluggable HAL interfaces for MCU portability:

| Interface | Purpose |
|-----------|---------|
| `osp_transport_t` | TCP/serial send/recv |
| `osp_crypto_t` | AES-GCM, MD5, SHA1, SHA256 |
| `osp_random_t` | Random number generation |
| `osp_timer_t` | Timing (now_ms, delay_ms) |
| `osp_system_t` | System title + key store |

See `examples/linux_hal.h/.c` for a complete Linux implementation (TCP + OpenSSL + POSIX timer).

## Security notes (production)

- Set **unique** `system_title`, keys, and monotonic **invocation counters** per association.
- Use **HLS** (not LLS) on untrusted links; prefer suite 1/2 or GOST suite 8/9 where required.
- **Dedicated keys**: transfer only inside authenticated+encrypted `glo-initiate-request` when ciphering is active.
- Replay protection: `osp_glo_unprotect` rejects non-increasing IC; server returns **exception-response** on IC/decipher errors.
- Wire real crypto HAL in production — bundled GOST is for portability/testing; use certified modules where mandated.

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
tests/           CMocka suites (14 CTest targets, 271 test functions)
examples/        loopback, TCP client/server, Linux HAL demo
```

## References

- IEC 62056-5-3 (xDLMS)
- IEC 62056-46 / -47 (HDLC / wrapper)
- ISO/IEC 13239 (HDLC frame format)
- R 1323565.1 (GOST transport & HLS)
- spodes-rs — API and test vector parity reference

## License

GPL-3.0-or-later. See [LICENSE](LICENSE).
