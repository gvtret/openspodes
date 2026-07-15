# OpenSPODES Security Guide

Comprehensive guide to DLMS/COSEM security in OpenSPODES, covering authentication, encryption, and key management.

## Overview

OpenSPODES implements the full IEC 62056-5-3 security model:

- **HLS (High-Level Security)**: 4-pass challenge-response authentication (mechanisms 0-10)
- **Glo-ciphering**: APDU-level encryption + authentication (AES-GCM or Kuznyechik)
- **Ded-ciphering**: Dedicated key encryption (per-association)
- **General ciphering**: Agreed-key variant for inter-device communication

## Security Suites

| Suite | Name | Cipher | Signature | Hash |
|-------|------|--------|-----------|------|
| 0 | AES-GCM-128 | AES-128-GCM | — | — |
| 1 | ECDH-ECDSA-AES-GCM-128-SHA-256 | AES-128-GCM | ECDSA-P256 | SHA-256 |
| 2 | ECDH-ECDSA-AES-GCM-256-SHA-384 | AES-256-GCM | ECDSA-P384 | SHA-384 |
| 8 | Kuznyechik | Kuznyechik-GCM | — | — |
| 9 | Kuznyechik+VKO+Streebog | Kuznyechik-GCM | VKO | Streebog-256 |

**Suite 0** is the simplest — no key agreement, no signatures. Suitable for:
- Lab/testing environments
- Networks with pre-provisioned symmetric keys
- Meters behind a trusted gateway

**Suite 8/9** (GOST) is required for SPODES deployments.

## HLS Authentication (4-Pass Handshake)

```
Client                                Server
  │                                     │
  │──── AARQ (CtoS challenge) ────────>│
  │                                     │ Compute f(CtoS)
  │<──── AARE (StoC challenge) ───────│
  │                                     │
  │ Compute f(StoC), send in pass 3 ──>│
  │                                     │ Verify f(StoC)
  │<──── f(CtoS) in pass 4 ───────────│
  │                                     │
  │ Verify f(CtoS)                      │
```

### Mechanism IDs

| ID | Name | Implementation |
|----|------|---------------|
| 0 | Lowest | No authentication |
| 1 | LLS | Password-based |
| 2 | HLS (no crypto) | Challenge-response without crypto |
| 3 | HLS-MD5 | MD5 hash (deprecated) |
| 4 | HLS-SHA1 | SHA-1 hash (deprecated) |
| 5 | HLS-GMAC | AES-GCM authentication tag |
| 6 | HLS-SHA256 | SHA-256 hash |
| 7 | HLS-ECDSA | ECDSA signature |
| 8 | HLS-GOST-CMAC | Kuznyechik CMAC |
| 9 | HLS-GOST-Streebog | HMAC-Streebog256 |
| 10 | HLS-GOST-Sig | GOST 34.10 signature |

### f(StoC) / f(CtoS) Computation

The `f()` function depends on the mechanism:

| Mechanism | f(x) |
|-----------|------|
| 3 | MD5(x) |
| 4 | SHA-1(x) |
| 5 | GMAC(StoC, system_title, IC) |
| 6 | SHA-256(x) |
| 7 | ECDSA-Sign(x) |
| 8 | CMAC-Kuznyechik(x) |
| 9 | HMAC-Streebog256(x) |
| 10 | GOST-Sign(x) |

## Key Types

| Key | ID | Size | Purpose |
|-----|-----|------|---------|
| GUEK | 0 | 16 bytes | Global Unicast Encryption Key |
| GAK | 1 | 16 bytes | Global Authentication Key |
| GBEK | 2 | 16 bytes | Global Broadcast Encryption Key |
| KEK | 3 | 16 bytes | Key Encryption Key |
| DEK | — | 16 bytes | Dedicated Encryption Key (per-association) |

### Key Usage

- **GUEK**: Used for glo-encryption (AES-GCM key)
- **GAK**: Used for GMAC authentication (AES-GCM key for auth-only mode)
- **DEK**: Installed via `InitiateRequest` when ciphering is active; replaces GUEK for the association

## Glo-Ciphering

APDU-level encryption using AES-GCM or Kuznyechik-GCM:

```
┌─────────────────────────────────────────────┐
│ Encrypted APDU (0xC8-0xCF for glo, 0xD0-0xD7 for ded) │
├─────────────────────────────────────────────┤
│ security_control_byte (1 byte)              │
│ authentication_key (16 bytes, AAD)          │
│ invocation_counter (4 bytes, AAD + IV)      │
│ system_title (8 bytes, IV)                  │
│ plaintext (encrypted)                       │
│ authentication_tag (12 bytes)               │
└─────────────────────────────────────────────┘
```

### Tag Mapping

| Plain tag | Glo tag | Ded tag |
|-----------|---------|---------|
| GET request (0xC0) | 0xC8 | 0xD0 |
| SET request (0xC1) | 0xC9 | 0xD1 |
| ACTION request (0xC3) | 0xCB | 0xD3 |
| GET response (0xC4) | 0xCC | 0xD4 |
| SET response (0xC5) | 0xCD | 0xD5 |
| ACTION response (0xC7) | 0xCF | 0xD7 |

## Invocation Counter

The invocation counter (IC) is a monotonically increasing 32-bit value used for:

1. **Replay protection**: Each APDU must have a higher IC than the previous one
2. **IV construction**: IC is part of the AES-GCM IV (bytes 8-11)
3. **Uniqueness**: Ensures each encryption operation uses a unique IV

**Overflow**: Per IEC 62056-5-3, when IC reaches 0xFFFFFFFF, the association must be re-keyed. OpenSPODES returns an error on overflow.

**Management**:
- Client and server each maintain separate IC counters (`cipher_tx.invocation_counter`)
- IC starts at 0 after association
- IC increments after each successful glo-protect/unprotect operation
- The peer's IC is tracked in `last_peer_ic` for replay detection

## General Ciphering (0xDB-0xDD)

General ciphering provides a different envelope format for inter-device communication:

```
┌─────────────────────────────────────────────┐
│ Tag 0xDD (general ciphering)                │
├─────────────────────────────────────────────┤
│ transaction_id (octet string)               │
│ sender_system_title (octet string)          │
│ recipient_system_title (octet string)       │
│ other_information (empty)                   │
│ key_info (empty or key negotiation)         │
│ ciphered_data (glo-protected body)          │
└─────────────────────────────────────────────┘
```

## Production Security Checklist

### Key Provisioning
- [ ] Unique 8-byte `system_title` per device (MAC, serial number, etc.)
- [ ] GUEK and GAK provisioned from secure key management system
- [ ] Keys stored in secure element or encrypted storage (not hardcoded!)
- [ ] Key rotation schedule defined

### Authentication
- [ ] Use HLS mechanism 5 (GMAC) minimum — never LLS on untrusted links
- [ ] For GOST deployments: use mechanism 8 or 9
- [ ] Verify challenge randomness (`osp_hal_random_fill` must use hardware TRNG)

### Encryption
- [ ] Enable glo-ciphering for all GET/SET/ACTION over untrusted links
- [ ] Use dedicated keys (DEK) for long-lived associations
- [ ] Monitor invocation counter — re-key before overflow

### Replay Protection
- [ ] Verify `osp_glo_unprotect` rejects decreasing IC (built-in)
- [ ] Log replay attempts for security monitoring

### Key Zeroization
- [ ] Call `osp_sec_context_destroy()` when association ends
- [ ] Verify keys are zeroized (protected by `memset` to 0)

### Transport Security
- [ ] Use HDLC over encrypted channel (TLS, VPN) or WRAPPER with glo-ciphering
- [ ] Never send unencrypted credentials over serial/TCP

## Thread Safety

The library is safe for multi-threaded use when:
1. Each thread uses its own `osp_client_t` / `osp_server_t`
2. `osp_hal_mutex` is set to a platform-appropriate mutex implementation
3. Internal crypto buffers and serialization pools use thread-local storage (`OSP_TLS`)

**TLS buffers:** All non-reentrant static arrays (HMAC inner buffer, IC value constructors, value_read_pool) are declared with `OSP_TLS`, which resolves to `_Thread_local` (C11), `__thread` (GCC/Clang), or plain `static` on bare-metal. This eliminates data races without requiring locks.

For bare-metal single-threaded applications, leave `osp_hal_mutex = NULL` for zero overhead. TLS falls back to static automatically.

## Debugging Failed Authentication

Common causes of HLS failure:

| Symptom | Likely Cause | Fix |
|---------|-------------|-----|
| AARE result = mechanism_mismatch | Server doesn't support requested mechanism | Use mechanism 0-10 that server supports |
| Pass 3 verify fails | Wrong GAK key or mismatched system_title | Verify keys match on both sides |
| Pass 4 verify fails | Server computed wrong f(CtoS) | Check server's key and hash implementation |
| IC error | Invocation counter mismatch | Ensure both sides increment IC correctly |
| Timeout on AARE | Network issue or server not responding | Check transport, increase timeout |

## References

- IEC 62056-5-3 (DLMS/COSEM Application Layer)
- IEC 62056-6-2 (COSEM Interface Classes)
- Green Book (DLMS/COSEM Security)
- GOST R 34.10-2018 (Digital Signature)
- GOST R 34.11-2012 (Hash Function)
- GOST R 34.12-2015 (Block Cipher — Kuznyechik)
