# Changelog

All notable changes to this project are documented in this file.

Format is based on [Keep a Changelog](https://keepachangelog.com/), project follows [Semantic Versioning](https://semver.org/).

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

## [1.1.0] - Unreleased

### Fixed (Security Audit)

- **CRITICAL**: Server challenge (stoc) was hardcoded as `ABCDEFGH`, defeating HLS challenge-response. Now uses `osp_hal_random_fill` for random bytes.
- **HIGH**: GMAC AAD buffer could read uninitialized stack memory when `data_len > 256`. Now clamped to 256.
- **HIGH**: No key zeroization anywhere. Added `osp_sec_context_destroy()` and zeroization of `kuz_round_keys`.
- **HIGH**: `ber_backpatch_length` memmove without bounds check. Added buffer size validation.
- **HIGH**: `bitstring_read` integer overflow on `len * 8` for large lengths. Added overflow check.
- **MEDIUM**: `skip_type_description` unbounded recursion. Added `OSP_MAX_TYPE_DEPTH` limit (16).
- **MEDIUM**: `osp_aarq_decode` unchecked `ber_read_oid` returns. Added error propagation.
- **MEDIUM**: `field_start + field_len` overflow in AARQ field skip. Added overflow check.
- **MEDIUM**: Duplicate `#define` in security.h removed.

### Changed (Build Hardening)

- Added `-Wall -Wextra -Werror -fstack-protector-strong` to library build.
- Added `_FORTIFY_SOURCE=2` compile definition.
- Removed dead `fe_add_raw` function from gost3410.c.

### Added (Documentation)

- Doxyfile for API reference generation.
- `@param`/`@return` doxygen for 15+ key public API functions.
- `@file` headers for security.h, gbt.h, notification.h.
- CHANGELOG translated to English.
- Removed `README_CN.md` (all documentation now in English only).
