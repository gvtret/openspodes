# OpenSPODES

Portable **C11** implementation of **IEC 62056 DLMS/COSEM** (LN referencing), modeled after [spodes-rs](https://github.com/gvtret/spodes-rs) and aligned with the **СПОДЭС / GOST** profile (R 1323565.1).

Designed for embedded and server use: **no heap allocation in the core library**, HAL crypto via function pointers, static buffers.

## Features

| Area | Status |
|------|--------|
| Codec (BER/AXDR, compact-array) | ✅ |
| Transport (HDLC, TCP wrapper) | ✅ |
| Client / Server session drivers | ✅ |
| GET / SET / ACTION (+ with-list, block transfer) | ✅ |
| General block transfer (unconfirmed + confirmed + **lost-block recovery**) | ✅ |
| glo- / ded-ciphering (AES-GCM + Kuznyechik suite 8/9) | ✅ |
| HLS (GMAC, MD5/SHA1/SHA256, GOST 8–10) | ✅ |
| general-ciphering / general-signing | ✅ |
| Push + event notifications | ✅ |
| 40 COSEM IC classes | ✅ |
| GOST (Streebog, Kuznyechik, GOST 34.10, VKO/KDF) | ✅ |

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

## Quick start (loopback)

```bash
./build/openspodes_loopback_cli demo
```

TCP wrapper example (port 4059):

```bash
./build/openspodes_tcp_server &
./build/openspodes_tcp_client
```

## Integration pattern

1. Provide `osp_transport_t` (open/send/recv/close).
2. Optionally wire HAL: `osp_hal_gcm_crypt`, hash functions, GOST hooks.
3. Initialize `osp_server_t` or `osp_client_t`, register IC objects (server).
4. Enable ciphering / GBT / dedicated key as needed:

```c
osp_sec_context_t tx, rx;
osp_sec_context_init(&tx, OSP_SUITE_0, OSP_MECH_HLS_GMAC, client_st);
/* load guek/gak, set policy, IC */
osp_client_set_ciphering(&client, &tx, &rx);
osp_client_set_dedicated_key(&client, dek, 16);  /* optional ded-ciphering */
osp_client_enable_gbt(&client, 64);
osp_client_connect(&client, 5000);
osp_client_get(&client, 1, &obis, 2, &value);
```

Server-side unsolicited notifications:

```c
osp_server_send_event_notification(&server, &event);
osp_ic_push_setup_trigger(...);  /* data-notification via IC 40 */
```

## Security notes (production)

- Set **unique** `system_title`, keys, and monotonic **invocation counters** per association.
- Use **HLS** (not LLS) on untrusted links; prefer suite 1/2 or GOST suite 8/9 where required.
- **Dedicated keys**: transfer only inside authenticated+encrypted `glo-initiate-request` when ciphering is active.
- Replay protection: `osp_glo_unprotect` rejects non-increasing IC; server returns **exception-response** on IC/decipher errors.
- Wire real crypto HAL in production — bundled GOST is for portability/testing; use certified modules where mandated.

## Layout

```
src/codec/      BER/AXDR, COSEM serialize
src/transport/  HDLC + wrapper
src/service/    xDLMS APDUs, ACSE, GBT, initiate
src/security/   HLS, glo/ded, GOST
src/client/     Session client
src/server/     Dispatcher + accept loop
src/ic/         Interface classes
tests/          CMocka suites (11 CTest targets)
docs/HANDOFF.md Living progress log for multi-session work
```

## References

- IEC 62056-5-3 (xDLMS)
- IEC 62056-46 / -47 (HDLC / wrapper)
- Р 1323565.1 (GOST transport & HLS)
- spodes-rs — API and test vector parity reference

## License

See repository license files.
