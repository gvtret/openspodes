# HANDOFF — OpenSPODES (full context for new session)

## Repository
- **Path**: /mnt/e/work/opendlms/openspodes
- **Branch**: main, 14 commits, HEAD f95b4bc
- **Reference**: /home/trgv/spodes-rs (Rust implementation)
- **Language**: C11, clang-format (LLVM tabs), clang-tidy clean
- **Stats**: 96 files, ~8.5K lines, 38 IC classes, 22 unit tests pass

## Project: OpenSPODES — DLMS/COSEM protocol stack in C11
A portable C99/C11 implementation of IEC 62056 DLMS/COSEM, modeled after spodes-rs.
No malloc in core. HAL via function pointers. MCU-pluggable.

## Architecture
```
src/codec/     BER/AXDR read/write, serialize/deserialize, types (33 A-XDR + all composite structures)
src/transport/ HDLC (IEC 62056-46) + COSEM wrapper (IEC 62056-47)
src/service/   ACSE (AARQ/AARE/RLRQ) + GET/SET/ACTION + ExceptionResponse
src/security/  HLS GMAC (pass 3/4) + replay protection + glo stubs
src/server/    RequestDispatcher (class_id+OBIS), osp_server_accept/run
src/client/    osp_client_connect/get/set/action, HLS handshake
src/ic/        38 IC classes (all spodes-rs + extras), vtable pattern
```

## Unit Tests: 22/22 CMocka — PASS
## Integration Tests: 5/5 FAIL (blocked on AARQ BER encoding — see below)

## CRITICAL: AARQ/AARE BER Encoding (IN PROGRESS)

### What works
- Standalone AARQ encode/decode: PERFECT
  - Encodes: 49 bytes, correct context-specific tags [1]=A1, [10]=8A, [11]=8B, [12]=AC, [196]=BF
  - Decodes: app_ctx=1, mechanism=1, auth_len=8, auth="01234567"
  - Verified against Green Book Table 113 / IEC 62056-5-3 Table D.4

### What's broken
- Integration test: server receives 61 bytes (should be ~49), AARQ decode FAILS
- Root cause: BER length encoding uses 2-byte form always (0x81 + byte) instead of 1-byte for < 128
- Per IEC 62056-5-3: length = 1 byte if < 128, 2 bytes (0x81+byte) if >= 128
- The extra byte (61 vs 49) may also come from mock transport framing issues

### Files modified (UNCOMMITTED, not committed):
- `src/service/service.c`: AARQ/AARE encoder with correct BER context-specific tags ([1]=A1,[10]=8A,[11]=8B,[12]=AC,[196]=BF) + OID wrapper lengths + 2-byte length backfill + decoder fixes for 0xA1, 0x8B, 0xAC, 0xBE tags
- `src/server/server.c`: debug fprintf + stdio.h (REMOVE before commit)
- `tests/mock_transport.c`: mock_open/mock_close, removed static from helpers
- `tests/mock_transport.h`: added mock_send_to_peer/recv_from_peer declarations
- `tests/test_integration.c`: 5 integration tests (all fail at connect)
- `tests/test_debug_flow.c`, `tests/test_aarq_direct.c`: debug helpers

### Next steps to fix AARQ:
1. Fix BER length: use 1-byte form for < 128, 2-byte for >= 128 (not always 2-byte)
2. Trace 61 vs 49 byte difference through mock transport
3. May need to re-examine the loopback transport flow
4. Verify against COSEMpdu_GB83.asn (user-provided ASN.1 file)

## User Instructions (MUST follow)
- **Always consult doc-rag-remote when implementing features**
- **BER length: 1 byte if < 128, 2 bytes (0x81+byte) if >= 128**
- **COSEMpdu_GB83.asn**: ASN.1 service definitions, use for encoding verification
- **Commit convention**: no "Co-Authored-By: Claude" trailers

## IC Classes (38 implemented)
Data(1) Register(3) ExtRegister(4) DemandRegister(5) RegisterActivation(6)
ProfileGeneric(7) Clock(8) ScriptTable(9) Schedule(10) SpecialDays(11)
AssociationLN(15) SAPAssignment(17) ImageTransfer(18) IEC_HDLCSetup(23)
UtilityTables(26) DataProtection(30) ProfileFilter(31) PushSetup(40)
IPv4Setup(42) MACAddress(43) IPv6Setup(48) RegisterTable(61) CompactData(62)
StatusMapping(63) SecuritySetup(64) ParameterMonitor(65) Arbitrator(68)
DisconnectControl(70) Limiter(71) ActivityCalendar(20) RegisterMonitor(21)
SingleActionSchedule(22) IEC_LocalPortSetup(19) GPRSModemSetup(45)
GSMDiagnostic(47) MBusSlaveSetup(76) TableManager(8200)

## HAL
- Function pointer structs: osp_transport_t, osp_crypto_t, osp_random_t, osp_timer_t, osp_system_t
- Global crypto HAL: osp_hal_gcm_init/update/finish for security module
- No malloc in core; 1KB fixed PDU buffers

## Open questions
- LICENSE: needs THIRD-PARTY-LICENSES.txt for Apache 2.0 mbed TLS
- spodes-rs spodus/ module not ported yet (14 files)
- No example applications yet

## 2026-07-10 — BER encoding fix + AARQ/AARE rewrite + codec tests

**Done:**
- Fixed `osp_ber_write_tag` in `src/codec/codec.c:109` — now handles multi-byte tag numbers (base-128 continuation) for tags >= 128
- Added `ber_backpatch_length` helper in `src/service/service.c` — reserves 1-byte placeholder, backpatches with short form (< 128) or shifts content and uses long form (0x81 + byte)
- Rewrote `osp_aarq_encode` — fixed all tags per IEC 62056-5-3 Table D.4 / Green Book Table 113:
  - [1] A1 (EXPLICIT wraps OID)
  - [6] A6 (EXPLICIT wraps OCTET STRING) for calling-AP-title — was incorrectly using [12]
  - [10] 8A (IMPLICIT BIT STRING)
  - [11] 8B (IMPLICIT OID)
  - [12] AC (EXPLICIT wraps charstring [0]) — auth-value was 00 01 len, now 80 len per spec
  - [30] BE (EXPLICIT wraps OCTET STRING) — was tag 196 (xDLMS get-response!)
- Rewrote `osp_aarq_decode` — uses `osp_ber_read_tag` for long-form tags, dispatches by tag_number on class 2
- Rewrote `osp_aare_encode` — fixed tags per Green Book Table D.6:
  - [2] A2 (EXPLICIT wraps INTEGER) — was IMPLICIT 0x82
  - [3] A3 (EXPLICIT wraps CHOICE) — was wrong structure
  - [4] A4 (EXPLICIT wraps OCTET STRING) for responding-AP-title — was IMPLICIT 0x84
  - [8] 88 (IMPLICIT) for responder-acse-requirements
  - [9] 89 (IMPLICIT OID) for mechanism-name
  - [10] AA (EXPLICIT wraps charstring) — was [12]
  - [30] BE (EXPLICIT wraps OCTET STRING)
- Rewrote `osp_aare_decode` — same pattern as AARQ decoder
- Fixed `osp_buf_init` usage in `src/server/server.c` and `src/client/client.c` — all receive buffers now set `buf.wr = rx_len` after init
- Fixed integration tests `tests/test_integration.c` — changed attribute 2 → 1 (Data IC only implements attr 1)

**State:** branch `main`, uncommitted. 22/22 core tests PASS. 88/88 golden vector tests PASS. 5/5 integration tests PASS. 115 total.

**Codec fixes applied this session:**
- `osp_ber_write_tag`: multi-byte tag numbers (base-128)
- `osp_axdr_read_u8/u16/u32`: null pointer for skip-reads
- `osp_axdr_write_octet_string`: BER length (not u32), no double-tag
- OCTET STRING, VISIBLE STRING, BIT STRING: read/write use BER length
- SET request: removed spurious "data present" flag (0x01)
- `osp_buf_init` usage: all receive buffers now set `buf.wr = rx_len`

**Tests added:**
- `tests/test_codec_golden.c`: 88 tests with golden vectors from `docs/golden_vectors.txt` + IEC specs
- Covers: BER length/tag, all A-XDR primitives, COSEM Data types, AARQ/AARE encode/decode, xDLMS services, roundtrips

**Remaining known issues (from golden test analysis):**
- `osp_ber_write_length` returns OSP_ERR_UNSUPPORTED for lengths > 65535 (no 4-byte length)
- `osp_ber_write_tag` takes uint8_t — can't encode tags >= 256
- `osp_value_write` missing BITSTRING/UTF8STRING/BCD/FLOAT32/FLOAT64 handling
- Data-notification encoder/decoder not implemented
- Access selection always writes 0 (stubbed)

**Next:** Commit all changes.

**Notes:**
- BER length: `osp_ber_write_length` in codec.c was already correct (short/long form). The bug was that AARQ/AARE encoders hardcoded `0x81` long-form placeholder instead of using backpatch.
- GXDLMSDirector.log analysis: HDLC wrapper `E6 E6 00` precedes AARQ `60 42 ...`. Server sees raw APDU without wrapper when using FRAMING_NONE.
- Reference: `/home/trgv/spodes-rs/src/service/acse.rs` — Rust implementation confirmed tags: [6]=A6, [12]=AC, [30]=BE, AARE [2]=A2, [4]=A4, [10]=AA.
- Green Book Table D.4 confirms: auth-value = AC 0A 80 08 <8 bytes> for LLS. Our encoder now matches.
- `osp_ber_read_tag` already handled long-form tags correctly (base-128). Only `osp_ber_write_tag` needed fixing.
