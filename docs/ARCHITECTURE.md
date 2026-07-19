# OpenSPODES Architecture

Comprehensive architecture documentation for the OpenSPODES library — a portable C11 implementation of the IEC 62056 DLMS/COSEM protocol stack.

## Overview

OpenSPODES implements the full DLMS/COSEM stack for communicating with electricity meters per IEC 62056 standards. The library is designed for embedded systems and servers: **zero heap allocation in the core**, crypto via function pointers, static buffers.

```
┌─────────────────────────────────────────────────────────┐
│                    Application (main)                    │
├─────────────────────────────────────────────────────────┤
│  client.c / server.c     SPODUS concentrator.c          │
│  (session client)        (session server)                │
├─────────────────────────────────────────────────────────┤
│  service/                 xdms.c, gbt.c, initiate.c     │
│  ACSE + xDLMS APDUs      notifications                  │
├─────────────────────────────────────────────────────────┤
│  security/                HLS, glo/ded, general-cipher   │
│  AES-GCM, Kuznyechik, GOST 34.10, Streebog              │
├─────────────────────────────────────────────────────────┤
│  ic/                      42 Interface Classes           │
│  ProfileGeneric, Data, Register, Clock, ...              │
├─────────────────────────────────────────────────────────┤
│  codec/                   BER + A-XDR + compact-array    │
│  serialize.c — serialization of all 33 COSEM types       │
├─────────────────────────────────────────────────────────┤
│  transport/               HDLC session + wrapper         │
│  hdlc_session.c           transport.c                    │
├─────────────────────────────────────────────────────────┤
│  HAL                      Transport, crypto, timer       │
│  osp_transport_t          osp_crypto_t                   │
│  osp_random_t             osp_timer_t                    │
└─────────────────────────────────────────────────────────┘
```

## Modules

### 1. Codec (`src/codec/`)

Core library — data encoding/decoding.

| File | Purpose |
|------|---------|
| `codec.c` | BER tag/length, A-XDR read/write for all primitive types |
| `serialize.c` | `osp_value_read/write` — generic CHOICE, 33 A-XDR tags |
| `ic_serialize.c` | IC object serialization via vtable |
| `types.h` | `osp_value_t` — tagged union (tag + data) |
| `structures.h` | Composite types: ProfileGeneric, Clock, Association, etc. |

**BER (Basic Encoding Rules):**
- Tag: 1-2 bytes (contiguous tags: 1 byte, context: 2 bytes)
- Length: 1 byte if < 128, 2 bytes (0x81+byte) if 128–65535
- Key functions: `osp_ber_read_tag`, `osp_ber_write_tag`, `osp_ber_read_length`, `osp_ber_write_length`

**A-XDR (Abstract Syntax Data Representation):**
- Primitives: u8, u16, u32, i8, i16, i32, i64, bool, octet_string, visible_string
- Generic: `osp_value_read/write` — handles all 33 tags
- Compact-array (tag 19) — optimized encoding for fixed-size types
- Structures/arrays: `osp_struct_begin`, `osp_array_begin`

### 2. Transport (`src/transport/`)

Two sublayers: HDLC (IEC 62056-46) and Wrapper (IEC 62056-47).

| File | Purpose |
|------|---------|
| `transport.c` | Frame/deframe HDLC, wrapper encode/decode, `osp_transport_send/recv_apdu` |
| `transport.h` | `osp_transport_t` — abstract transport (function pointers) |
| `hdlc_session.c` | HDLC session layer: SNRM/UA, N(S)/N(R), LLC, DISC/DM, XID |
| `hdlc_session.h` | `osp_hdlc_session_t` — HDLC link state machine |

**HDLC (IEC 62056-46):**
- Frame format type 3 (ISO/IEC 13239): flag(7E) + format(2) + addr + control + [HCS] + info + FCS + flag
- FCS: CRC-16/X.25 (polynomial 0x8408, init 0xFFFF)
- Control field: I-frame [N(R):3][P/F][N(S):3][0], S-frame [N(R):3][P/F][mod:2][01], U-frame [mod_hi:3][P/F][mod_lo:2][11]
- U-frame types: SNRM(0x83), UA(0x63), DISC(0x43), DM(0x0F), FRMR(0x87), UI(0x03), XID(0xBF)

**HDLC Session Layer:**
```
IDLE ──connect──► CONNECTING ──UA received──► CONNECTED
                         │                           │
                      timeout                      disconnect
                         │                           │
                         ▼                           ▼
                       IDLE ◄──── DISC/DM ──── DISCONNECTING
```

**Wrapper (IEC 62056-47):**
- 8-byte header: version(2) + source(2) + destination(2) + length(2)
- `osp_wrapper_encode/decode`

### 3. Service Layer (`src/service/`)

DLMS/COSEM protocol — ACSE + xDLMS APDUs.

| File | Purpose |
|------|---------|
| `service.c` | BER codecs for AARQ/AARE/RLRQ/RLRE |
| `xdms.c` | GET/SET/ACTION request/response encode/decode |
| `xdms_selective.c` | Selective access encode/decode |
| `gbt.c` | General Block Transfer (tag 0xE0) |
| `initiate.c` | InitiateRequest/InitiateResponse |
| `notification.c` | Event/Data notifications |

**ACSE (IEC 62056-5-3):**
- AARQ (0x60): application_context, mechanism, calling_ap_title, auth_value, user_info
- AARE (0x61): result, source_diagnostic, responding_ap_title, auth_value, user_info
- RLRQ/RLRE: release

**xDLMS APDUs:**
- GET: normal, with-block, with-list
- SET: normal, with-first-datablock, with-datablock, with-list
- ACTION: normal, with-first-param-block, with-param-block, with-list
- Response types: data, data-error, block, block-last, with-list

**General Block Transfer (GBT):**
- Unconfirmed (window=0) and confirmed (window>0) modes
- Lost-block recovery
- STR bit for streaming

### 4. Security (`src/security/`)

| Module | Purpose |
|--------|---------|
| `security.c` | HLS mechanisms 0-10, glo/ded ciphering |
| `general_ciphering.c` | general-ciphering/signing APDUs |
| `kuznyechik.c` | Kuznyechik block cipher (GOST 34.12-2015) |
| `gost3410.c` | GOST 34.10-2018-256 digital signatures |
| `streebog/` | Streebog-256 hash (GOST 34.11-2012) |

**HLS Mechanisms:**
| ID | Name | Implementation |
|----|------|---------------|
| 0 | Lowest (no authentication) | N/A |
| 1 | LLS | Password |
| 2 | HLS (no crypto) | Challenge |
| 3 | HLS-MD5 | `osp_hal_md5` |
| 4 | HLS-SHA1 | `osp_hal_sha1` |
| 5 | HLS-GMAC | `osp_hal_gcm_crypt` |
| 6 | HLS-SHA256 | `osp_hal_sha256` |
| 7 | HLS-ECDSA | `osp_hal_ecdsa_sign/verify` |
| 8 | HLS-GOST-CMAC | Built-in (Kuznyechik) |
| 9 | HLS-GOST-Streebog | HMAC-Streebog256 |
| 10 | HLS-GOST-Sig | GOST 34.10 sign/verify |

**APDU Ciphering:**
- glo (0xC8-0xCF): auth + encrypt via AES-GCM or Kuznyechik-CTR-CMAC
- ded (0xD0-0xD7): dedicated key from InitiateRequest
- general-ciphering (0xDB-0xDD): agreed-key variant
- general-signing (0xDF): GOST 34.10 signature

**Suite 8/9 (GOST):**
- Kuznyechik GCM for APDU protection
- VKO (R 50.1.113-2016) for key agreement
- KDF_TREE based on Streebog-256

### 5. IC Classes (`src/ic/`)

42 COSEM Interface Classes (Blue Book):

| class_id | Class | Status |
|----------|-------|--------|
| 1 | Data | ✅ get/set |
| 3 | Register | ✅ get |
| 4 | Extended Register | ✅ get |
| 5 | Demand Register | ✅ get |
| 6 | Register Activation | ✅ get |
| 7 | Profile Generic | ✅ get + selective access |
| 8 | Clock | ✅ get/set |
| 9 | Script Table | ✅ get |
| 10 | Schedule | ✅ get |
| 11 | Special Days | ✅ get |
| 15 | Association LN | ✅ get/set |
| 17 | SAP Assignment | ✅ get/set |
| 18 | Image Transfer | ✅ get/invoke |
| 19 | IEC Local Port Setup | ✅ get/set |
| 20 | Activity Calendar | ✅ get/set |
| 21 | Register Monitor | ✅ get |
| 22 | Single Action Schedule | ✅ get |
| 23 | IEC HDLC Setup | ✅ get/set |
| 25 | MBus Slave Port Setup | ✅ get/set |
| 26 | Utility Tables | 🔵 stub |
| 30 | Data Protection | ✅ get |
| 31 | Profile Filter | 🔵 stub |
| 40 | Push Setup | ✅ get/set |
| 41 | TCP/UDP Setup | ✅ get/set |
| 42 | IPv4 Setup | ✅ get/set |
| 43 | MAC Address | ✅ get |
| 45 | GPRS Modem Setup | ✅ get/set |
| 47 | GSM Diagnostic | ✅ get |
| 48 | IPv6 Setup | ✅ get/set |
| 61 | Register Table | 🔵 stub |
| 62 | Compact Data | ✅ get/set |
| 63 | Status Mapping | 🔵 stub |
| 64 | Security Setup | ✅ get/invoke |
| 65 | Parameter Monitor | 🔵 stub |
| 68 | Arbitrator | ✅ get |
| 70 | Disconnect Control | ✅ get/set |
| 71 | Limiter | ✅ get/invoke |
| 76 | MBus Slave Setup | 🔵 stub |
| 8200 | Table Manager | ✅ get/set/invoke |
| 8201 | Profile Data Filter | ✅ get/set/invoke |

### 6. Client/Server (`src/client/`, `src/server/`)

**Client** (`client.c`):
```
osp_client_init()
  → osp_client_connect()         AARQ → AARE → HLS pass3/4
  → osp_client_get()             GET request/response
  → osp_client_get_with_list()   GET with-list
  → osp_client_set()             SET request/response
  → osp_client_set_with_list()   SET with-list
  → osp_client_action()          ACTION request/response
  → osp_client_action_with_list() ACTION with-list
  → osp_client_get_with_selective_access()  GET + date/entry filter
  → osp_client_release()         RLRQ → RLRE
  → osp_client_disconnect()      DISC/UA (HDLC) + transport close
```

**Server** (`server.c`):
```
osp_server_init()
  → osp_server_register()        Register IC objects
  → osp_server_accept()          Loop: AARQ→AARE, GET/SET/ACTION, RLRQ→RLRE
  → osp_server_run()             Convenience loop
  → osp_server_send_event_notification()  Push event notification
```

**Dispatcher** (`dispatcher.c`):
- Routing by (class_id, OBIS)
- ACL checking via Association LN
- Maximum 64 objects

### 7. SPODUS Concentrator (`src/spodus/`)

SPODUS IVCV implementation (STO 34.01-5.1):

| File | Purpose |
|------|---------|
| `concentrator.c` | Runtime: registry, downstream connections |
| `meter_registry.c` | Meter registry + reading cache (§10.2) |
| `direct_channel.c` | Direct channel table (§10.3) |
| `channel_list.c` | ProfileGeneric channel list (§10.4) |
| `discovered.c` | Discovered meters ProfileGeneric (§10.5) |
| `access_policy.c` | Access policies (§10.6) |
| `tasks.c` | Data exchange tasks (§10.7) |
| `spodus_server.c` | Data IC for server registration |

### 8. HAL (`src/openspodes.h`)

Hardware Abstraction Layer — interfaces for MCU porting:

| Interface | Purpose |
|-----------|---------|
| `osp_transport_t` | open/send/recv/close/is_connected + ctx |
| `osp_crypto_t` | AES-GCM: init/update/finish/free |
| `osp_random_t` | fill(buf, len) |
| `osp_timer_t` | now_ms() + delay_ms(ms) |
| `osp_system_t` | system_title[8] + get_key(sap, key_id) |

## Data Flows

### Client: GET over HDLC

```
osp_client_get()
  → client_send_apdu()
    → [glo encrypt if enabled]
    → osp_hdlc_session_send_apdu()
      → osp_hdlc_frame() + transport->send()
  → client_recv_apdu()
    → osp_hdlc_session_recv_apdu()
      → transport->recv() + osp_hdlc_deframe()
    → [glo decrypt if enabled]
  → osp_get_response_decode()
```

### Server: handling incoming request

```
osp_server_accept()
  → osp_transport_recv_apdu()  (or HDLC session recv)
  → tag detection:
    0x60 → handle_aarq()
    OSP_ACSE_RLRQ_TAG → handle_rlrq()
    OSP_TAG_GET_REQUEST → handle_get()
    OSP_TAG_SET_REQUEST → handle_set()
    OSP_TAG_ACTION_REQUEST → handle_action()
  → response encode + server_send()
```

### HDLC session: SNRM/UA

```
Client:                              Server:
  osp_hdlc_session_connect()
    → send SNRM + XID info
                                    osp_hdlc_session_connect()
                                      → recv SNRM
                                      → decode XID, negotiate params
                                      → send UA + XID info
    → recv UA
    → decode XID, negotiate params
    → state = CONNECTED
```

## Constants and Limits

| Constant | Value | Description |
|----------|-------|-------------|
| `OSP_CLIENT_MAX_PDU` | 1024 | Maximum client APDU |
| `OSP_SERVER_MAX_PDU` | 1024 | Maximum server APDU |
| `OSP_CLIENT_REASSEMBLE_MAX` | 4096 | Reassembly buffer |
| `OSP_CLIENT_BLOCK_SIZE` | 64 | Block transfer block size |
| `OSP_HDLC_MAX_FRAME_SIZE` | 512 | Maximum HDLC frame |
| `OSP_HDLC_MAX_ADDR_LEN` | 4 | Maximum HDLC address length |
| `OSP_SPODUS_MAX_METERS` | 16 | Maximum meters in concentrator |
| `OSP_MAX_OBJECTS` | 320 | Maximum IC objects in dispatcher |
| `OSP_MAX_BUFFER_ROWS` | 64 | Maximum rows in ProfileGeneric buffer |
| `OSP_MAX_CAPTURE_OBJECTS` | 8 | Maximum capture objects |

## Testing

| Suite | File | Tests | Coverage |
|-------|------|-------|----------|
| `test_core` | test_core.c | 30 | BER/AXDR codec, selective access, IC vtable |
| `test_golden` | test_codec_golden.c | 104 | Golden vectors +  |
| `test_errors` | test_errors.c | 34 | Error paths |
| `test_ic` | test_ic_smoke.c | 3 | Smoke test for all 42 IC classes |
| `test_integration` | test_integration.c | 26 | E2E client↔server loopback |
| `test_phase0` | test_phase0.c | 7 | SPODUS helpers |
| `test_phase1` | test_phase1.c | 8 | Table manager |
| `test_phase2` | test_phase2.c | 13 | With-list, blocks, GBT |
| `test_spodus` | test_spodus_concentrator.c | 6 | Registry, proxy, poll |
| `test_gost` | test_gost_crypto.c | 14 | Streebog, Kuznyechik, GOST 34.10 |
| `test_security` | test_security_glo.c | 3 | glo/ded roundtrip |
| `test_hdlc` | test_hdlc_session.c | 18 | HDLC codec + session |
| `test_framing` | test_hdlc_wrapper_framing.c | 10 | WRAPPER + HDLC E2E |

## Build and Run

```bash
# Build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)

# Tests
ctest --test-dir build --output-on-failure

# Examples
./build/openspodes_loopback_cli demo
./build/openspodes_tcp_server &
./build/openspodes_tcp_client
./build/openspodes_linux_demo 127.0.0.1:4059
./build/openspodes_spodus_demo
./build/openspodes_serial_client /dev/ttyUSB0 9600

# API Documentation
doxygen Doxyfile
# → docs/api/html/index.html
```

## Dependencies

- **Required**: CMake ≥ 3.16, C11 compiler
- **Optional**: OpenSSL (for AES-GCM/ECDSA in tests and examples)
- **Testing**: CMocka (fetched automatically via FetchContent)
- **Documentation**: Doxygen (optional)
