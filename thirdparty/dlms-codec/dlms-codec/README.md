# DLMS/COSEM codec (BER + A-XDR + xDLMS LN services)

Allocation-free, cursor-based encoders/decoders for DLMS/COSEM, written in C11.
Symmetric for client and server; streaming (pull) decode suited to embedded.

## Layout

    dlms_codec.h    Public API: cursors, length codec, BER TLV, A-XDR COSEM `Data`.
    ber.c           Shared length codec, BER tag/TLV, APDU-encoding dispatch.
    axdr.c          A-XDR codec for the COSEM `Data` type (encode + pull-decode + walk).
    xdlms.h/.c      Streaming xDLMS LN services (GET/SET/ACTION/DataNotification)
                    layered on public A-XDR primitives.
    test.c          194 checks: golden vectors + round-trip + error paths.
    golden_gen.c    Emits golden_vectors.txt straight from the codec.
    golden_vectors.txt  Reference dumps (label + hex) for every vector.
    demo.c          A-XDR Data + BER structural demo.
    demo_xdlms.c    Full GET round trip across both roles, streaming only.

## Build & test

    make            # builds demo, demo_xdlms, test, golden_gen
    make check      # runs the test suite (non-zero exit on any failure)
    make golden     # regenerates golden_vectors.txt from the codec
    make libdlmscodec.a   # static library (ber.o axdr.o xdlms.o)
    make clean

Flags: `-std=c11 -Wall -Wextra -Werror -O2`. No dynamic allocation anywhere;
all I/O is bounds-checked over caller-provided buffers.

## Encoding dispatch (BER vs A-XDR)

You cannot tell BER from A-XDR by the bytes; dispatch on the leading APDU tag:
ACSE APDUs (AARQ 0x60 / AARE 0x61 / RLRQ 0x62 / RLRE 0x63) are BER; xDLMS
service APDUs (0xC0.. get/set/action, 0x0F data-notification, ...) are A-XDR.
See `dlms_encoding_for_apdu()`.

## Streaming contract

`*_begin`-style encoders write a PDU header and leave the writer exactly where
the next payload element must be appended; the doc comment on each states what
to append (a `Data` via `axdr_encode_data`, or a list). Decoders leave the
reader positioned at the `Data`/list, which you pull with
`axdr_decode_data` / `axdr_walk`.

## Wrapped services

GET request (normal / next / with-list), GET response (normal-data /
normal-result / with-datablock / with-list), SET request/response (normal),
ACTION request/response (normal), Data-Notification, Event-Notification,
Exception-Response. Plus building blocks (attr-desc-with-selection,
Get-Data-Result) for composing the remaining list variants.

COSEM `Data`: all scalar types, array, structure, and compact-array
(encode via `axdr_encode_compact_array`, decode/expand via `axdr_walk` /
`axdr_compact_array_walk`), including nested structure/array element types.

## Known limitations (by design)

- `ber_begin` writes long-form length (valid BER, not minimal/DER form).
- Ciphering (glo-/ded-/general-) and full ACSE AARQ/AARE field wrapping are
  out of scope; the codec handles the plaintext inner APDUs.
- Datablock (long GET/SET/ACTION) has PDU-level wrappers but no block
  reassembly; that is a separate state-machine layer.
- SET/ACTION with-list and *-datablock variants, and Access-Request/Response,
  are not one-call wrapped yet (primitives are available to build them).
- COSEM `Data` type tags (0..27) reflect the DLMS Blue Book; verify against your
  copy and, ideally, against real captures before relying on them.

## Verifying against a real stack

The vectors prove internal consistency and match the ASN.1 interpretation in
COSEMpdu_GB83; they do NOT prove agreement with another stack. Diff
`golden_vectors.txt` against Gurux / a DLMS analyzer on a few PDUs, or feed a
real capture in, to close that gap.
