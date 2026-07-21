# Changelog

All notable changes to this project are documented in this file.

Format is based on [Keep a Changelog](https://keepachangelog.com/), project follows [Semantic Versioning](https://semver.org/).

## [2.4.0] - 2026-07-21

### Fixed (Blue Book Compliance)

**Data IC (class_id=1) method 1:**
- Changed from "return value" to "reset" (set value to null) per Blue Book 4.3.1.3.1
- Updated integration tests to match new behavior

**Association LN (class_id=15) method 2:**
- Added `change_HLS_secret` — updates secret and secret_len from param

**Schedule IC (class_id=10) methods 1-3:**
- Method 1: enable/disable — toggle enable flag by index
- Method 2: insert — add new entry with start_time, end_time, scripts
- Method 3: delete — remove entry by index with array shift
- Added `enable` field to `osp_schedule_entry_t` structure

**Tests:**
- Updated `test_client_with_list` and `test_action_return_param_blocks` for Data IC method 1 change
- All 30/30 CTest targets pass

## [2.3.0] - 2026-07-20

### Added

**Data HAL Interface (`src/data_hal.h`):**
- `osp_hal_data_t` — generic HAL for reading current data, writing setpoints, executing commands
- Three callbacks: `read(obis, attr_id)`, `write(obis, attr_id, value)`, `execute(obis, method_id, param)`
- Global pointer `osp_hal_data` — set before server loop, NULL = no hardware access (default)
- `osp_hal_data_poll()` — scan dispatcher objects, refresh IC caches from HAL
- HAL + cache strategy: polling updates caches, DLMS GET returns cached values

**IC Class HAL Delegation (all 40 classes):**
- Every IC class now delegates get_attr/set_attr/invoke to HAL when `osp_hal_data != NULL`
- Transparent fallback: if HAL returns OSP_ERR_NOT_FOUND, IC uses its cached value
- Zero memory overhead — global pointer, no per-instance storage

**Tests:**
- `tests/test_data_hal.h` + `test_data_hal_db.c` — simple Dataset/Datapoint DB for testing
- `tests/test_data_hal.c` — 12 tests: null HAL, read/write/execute, IO error, NOT_FOUND, poll, integration, password from DB
- `openspodes_test_data_hal` CTest target

## [2.2.0] - 2026-07-20

### Fixed (HDLC Session Hardening)
- Inter-octet timeout (20–6000 ms) and inactivity timeout (0–120 s) per IEC 62056-46
- RX pending buffer for coalesced TCP reads — correct frame parsing from buffer
- I-frame segmentation reassembly (format-field S bit)
- DISC handling: NRM→UA+NDM, NDM→DM (correct lifecycle)
- FRMR response (W/X/Y/Z reason bits) per ISO 13239
- State/timeout callbacks for host integration
- XID negotiation: server resets to configured ceiling on new SNRM
- Bad FCS/HCS frames silently dropped with retry (DISC may follow)

### Fixed (Security)
- HLS-GMAC: AES-GCM key changed from GAK to GUEK (AAD still carries GAK) — aligns with Gurux/DLMS
- HLS mechanism 2: maps generic HLS to GMAC (5) for Suite 0 via `osp_hls_effective_mechanism()`
- Glo IV: TX uses local system_title, RX uses peer_system_title (fallback to local if zero)
- Fresh AA: IC replay state reset, peer title adopted from calling-AP-title
- Calling-AP-title validation: must be exactly 8 octets for ciphering and title-bound HLS

### Fixed (AARQ/AARE Protocol)
- Centralized `aarq_validate()` with structured ACSE diagnostics (0x0E auth required, 0x0D auth failure, 0x02 app context, 0x03 calling-AP-title, 0x0B mech not recognised)
- AARE: protocol-version echo, conditional auth fields (LLS omits 88/89/AA for SPODES etalon)
- Malformed GET/SET: returns DAR response instead of session drop for associated clients
- HLS HIGH Galois multiply: explicit 8-bit truncation matching Gurux `GXCipher::GaloisMultiply`

### Changed
- `OSP_HDLC_MAX_FRAME_SIZE` increased from 512 to 1024 (configurable via #ifndef, matches `OSP_SERVER_MAX_PDU`)
- `OSP_SERVER_PENDING_MAX` increased from 4KB to 16KB (×16 for ALN object_list with ACLs)
- `OSP_MAX_OBJECTS` default increased from 64 to 320 in dispatcher.h
- `server_max_receive_pdu_size` default increased from 0x01F4 to 0x0800

### Added
- `osp_hls_effective_mechanism()` — mechanism-2 to GMAC mapping
- `osp_hdlc_session_set_state_callback()` / `osp_hdlc_session_set_timeout_callback()`
- `osp_server_clear_ciphering()` / `osp_server_clear_current_association()`
- `osp_initiate_error_encode()` — ConfirmedServiceError for initiate rejection
- `OSP_ACSE_DIAG_CALLING_AP_TITLE_NOT_RECOGNIZED` (3)
- `test_glo_peer_iv_decrypt` — peer system title IV test

## [2.1.0] - 2026-07-17

### Added (Yellow Book Conformance Tests)

**ATS_DL V5 — Data Link Layer (HDLC):**
- `test_yb_ats_dl_hdlc_frame.c` — 13 tests: RR/RNR/REJ/FRMR/UI/XID codec roundtrips, bit-stuffing, escape sequences, HCS/FCS verification, N(S)/N(R) ranges
- `test_yb_ats_dl_hdlc_address.c` — 6 tests: 1–4 byte address encoding, broadcast, extension bit, multi-byte in full frames
- `test_yb_ats_dl_hdlc_ndm2nrm.c` — 5 tests: SNRM/UA exchange, XID parameter negotiation, connect timeout, reconnect cycle
- `test_yb_ats_dl_hdlc_info.c` — 8 tests: sequential I-frames, N(S)/N(R) mod-8 wrapping, windowed I-frames, RR/REJ handling, P/F bit
- `test_yb_ats_dl_hdlc_ndmop.c` — 5 tests: DISC in NDM, XID in disconnected mode, FRMR (skipped), UI rejection

**ATS_AL_COSEM_SYMSEC_0 V1.3 — Application Layer:**
- `test_yb_ats_al_appl_open.c` — 6 tests: AARQ/AARE with lowest/HLS mechanisms, unknown mechanism rejection, reconnect, wrong key
- `test_yb_ats_al_appl_data.c` — 6 tests: GET on non-existent OBIS/attr/method, SET on non-existent attr, with-list partial failure, unassociated request
- `test_yb_ats_al_appl_rel.c` — 4 tests: normal RLRQ/RLRE, GET after release, immediate DISC, reconnect cycle
- `test_yb_ats_al_appl_sec.c` — 4 tests: HLS GMAC handshake, wrong key (skipped), wrong system title, no shared secret
- `test_yb_ats_al_cosem_objs.c` — 2 tests: Data IC read/write
- `test_yb_ats_al_symsec_0.c` — 4 tests: GLO GET/SET with Suite 0, IC monotonic increment, IC overflow

**Infrastructure:**
- `tests/yb_helpers.c/h` — shared loopback setup boilerplate for all Yellow Book tests
- CMakeLists.txt: 11 new CTest targets (`yb_test_*`), OpenSSL-dependent targets gated behind `OSP_HAVE_OPENSSL_GCM`

**Notes:**
- FRMR generation/handling tests skipped pending implementation in `hdlc_session.c`
- RNR flow control test skipped pending implementation
- HLS key-verification tests skipped with mock crypto (require real OpenSSL)

## [2.0.1] - 2026-07-16

### Fixed

- Replaced C99 designated initializers with C11-compatible code for GCC 15 C++ compatibility (PR #1)
- Updated HANDOFF.md for v2.0.0 session

## [1.0.0] - 2026-07-11

### Added

**Transport:**
- HDLC session layer: SNRM/UA with XID parameters, N(S)/N(R) sequence tracking, LLC (`E6 E6 00`/`E6 E7 00`), DISC/DM
- Serial transport (UART/RS-485): 8N1, configurable baud, RTS control
- Examples: serial client/server with full DLMS flow over HDLC

**Codec:**
- Selective access encode/decode for ProfileGeneric (by date, by entry)
- `osp_client_get_with_selective_access()` — client function with filtering

**Service:**
- Server-side ProfileGeneric buffer filtering by selective access
- GET request encode/decode now use selective access parameters

**Examples:**
- `linux_hal` — full Linux HAL (TCP, OpenSSL, timer, random)
- `spodus_demo` — SPODUS concentrator with 3 meters

**Documentation:**
- `docs/ARCHITECTURE.md` — comprehensive architecture description
- Doxygen comments for IC classes: Data, Register, ProfileGeneric, Clock, AssociationLN, SAPAssignment, PushSetup, SecuritySetup, DisconnectControl
- `examples/serial_transport.h/.c` — documented Serial transport

**Infrastructure:**
- ASAN + UBSan CI job
- Coverage report (76.4% lines, 85.7% branches)
- `ENABLE_ASAN` cmake option

### Fixed

- **UB**: `serialize.c` — int64 overflow on 56-bit shift (replaced with uint64_t)
- **UB**: `general_ciphering.c` — null pointer memcpy on octet strings
- **Bug**: `test_spodus_concentrator` — dangling pointer (data_obj was local)
- **Bug**: `test_errors` — dangling pointer (g_error_data_obj was local)
- **ASAN false positive**: `osp_obis_eq` — added `__attribute__((noinline))`
- Coverage script: fixed path `build-cov` instead of `build-coverage`
- Coverage script: support `.o` files (Linux) in addition to `.obj` (Windows)
- `add_compile_options` moved before `add_library` for correct coverage instrumentation

### Changed
- License changed from MIT/Apache-2.0 to GPL-3.0-or-later

## [1.10.0] - 2026-07-15

### Fixed (Security Audit)

- **CRITICAL**: Server challenge (stoc) was hardcoded as `ABCDEFGH`, defeating HLS challenge-response. Now requires `osp_hal_random_fill` — rejects association without RNG.
- **HIGH**: No key zeroization anywhere. Added `osp_memzero()` with `memset_s`/`volatile` fallback. All key material now zeroized in `osp_sec_context_destroy`, `osp_sec_rotate_keys`, `osp_sec_cipher_apply_dedicated_key`, `osp_gost3410_sign`, `osp_gost_kuznyechik_cmac`, `osp_gost_hmac_streebog256`.
- **HIGH**: 65KB stack buffer in `streebog_wrap.c` HMAC. Moved to thread-local storage (`OSP_TLS`).
- **HIGH**: `ber_read_uint` had no overflow check — read all remaining buffer bytes. Added 4-byte limit.
- **HIGH**: `osp_ber_read_uint` overflow check prevents uint32 overflow on crafted input.

### Changed (Reentrancy)

- Added `OSP_TLS` macro (C11 `_Thread_local` / GCC `__thread` / bare-metal `static`).
- All mutable static buffers in crypto and IC modules converted to `OSP_TLS`.
- Library is now thread-safe: each thread can use its own `osp_client_t`/`osp_server_t` without data races.

### Changed (Portability)

- `gost3410.c`: Replaced `__uint128_t` (GCC extension) with portable `OSP_U128_MUL`/`OSP_U128_ADD3` macros. Fallback path works on MSVC and embedded compilers.
- `types.h`: Fixed empty struct `osp_null_t` for `-Wpedantic` compliance.
- Version constant `OPENSPODES_VERSION` aligned to 1.9.0 across `openspodes.h`, `CMakeLists.txt`, `README.md`.

### Changed (Code Quality)

- Removed all `goto` statements from codebase (`streebog_wrap.c`, `server.c`, `security.c`).
- `OSP_MAX_OBJECTS` increased from 32 to 64 to match documentation.
- Coverage script fixed: `build-cov` → `build-coverage`, added "Fully Covered" section.

### Added (Tests)

- 108 unit tests in `test_core.c` (was 33): selective access, SPODUS tasks/helpers/server, initiate, general ciphering, script table, SAP assignment, status mapping.
- `openspodes_test` now links `mock_crypto.c` + `real_crypto.c` (OpenSSL) for GCM testing.
- Coverage: **80.4% lines, 92.0% branches** (was 76.4% / 85.7%).

### Documentation

- `docs/HAL.md`: Added "Thread Safety" section with TLS explanation.
- `docs/SECURITY.md`: Updated thread safety section.
- `README.md`: Removed nonexistent `CONTRIBUTING.md` reference, fixed IC class count (36 full + 6 stub), updated test count (17 suites).
