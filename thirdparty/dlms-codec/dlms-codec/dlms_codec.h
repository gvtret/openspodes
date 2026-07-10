/* dlms_codec.h -- allocation-free BER (TLV) and A-XDR codecs for DLMS/COSEM.
 *
 * Design goals:
 *   - No dynamic allocation. All I/O over caller-provided buffers via cursors.
 *   - Strict bounds checking; every function returns a dlms_status.
 *   - Portable: explicit big-endian (network order) serialization, no reliance
 *     on host endianness. IEEE-754 assumed for float32/float64.
 *   - BER is used for ACSE (AARQ/AARE/RLRQ/RLRE); A-XDR for xDLMS APDUs and
 *     the COSEM `Data` type. You dispatch which one to use by the APDU tag.
 *
 * C11.
 */
#ifndef DLMS_CODEC_H
#define DLMS_CODEC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ status */
typedef enum {
    DLMS_OK = 0,
    DLMS_ERR_TRUNCATED,   /* not enough input bytes                          */
    DLMS_ERR_OVERFLOW,    /* not enough room in output buffer               */
    DLMS_ERR_BAD_LENGTH,  /* malformed length field                        */
    DLMS_ERR_UNSUPPORTED, /* indefinite length in A-XDR, compact-array, ... */
    DLMS_ERR_BAD_TAG,     /* unexpected / unknown tag                       */
    DLMS_ERR_RANGE        /* value does not fit target type                 */
} dlms_status;

const char *dlms_strerror(dlms_status s);

/* ------------------------------------------------------------- cursors */
typedef struct {
    const uint8_t *buf;
    size_t len;
    size_t pos;
} dlms_reader;

typedef struct {
    uint8_t *buf;
    size_t cap;
    size_t pos;
} dlms_writer;

static inline void dlms_reader_init(dlms_reader *r, const uint8_t *b, size_t n) {
    r->buf = b; r->len = n; r->pos = 0;
}
static inline void dlms_writer_init(dlms_writer *w, uint8_t *b, size_t cap) {
    w->buf = b; w->cap = cap; w->pos = 0;
}
static inline size_t dlms_remaining(const dlms_reader *r) { return r->len - r->pos; }

/* =====================================================================
 *  Length codec (shared short/long form; BER additionally has indefinite)
 * ===================================================================== */

/* Write minimal-length definite length. Used by both BER and A-XDR. */
dlms_status dlms_write_len(dlms_writer *w, uint32_t len);

/* Read a definite length (short or long form). Rejects indefinite. */
dlms_status dlms_read_len(dlms_reader *r, uint32_t *len_out);

/* BER-only: read a length, reporting the indefinite form (0x80) instead of
 * erroring. When *indefinite is true, *len_out is 0 and content is
 * terminated by end-of-contents (0x00 0x00). A-XDR must NOT use this. */
dlms_status ber_read_len(dlms_reader *r, uint32_t *len_out, bool *indefinite);

/* =====================================================================
 *  BER (Basic Encoding Rules) -- structural TLV, used for ACSE APDUs
 * ===================================================================== */

enum {
    BER_CLASS_UNIVERSAL   = 0x00,
    BER_CLASS_APPLICATION = 0x40,
    BER_CLASS_CONTEXT     = 0x80,
    BER_CLASS_PRIVATE     = 0xC0
};

/* DLMS ACSE APDU tags (application class, constructed). */
enum {
    ACSE_AARQ = 0x60, /* [APPLICATION 0] */
    ACSE_AARE = 0x61, /* [APPLICATION 1] */
    ACSE_RLRQ = 0x62, /* [APPLICATION 2] */
    ACSE_RLRE = 0x63  /* [APPLICATION 3] */
};

typedef struct {
    uint8_t  cls;         /* one of BER_CLASS_*                          */
    bool     constructed; /* P/C bit                                     */
    uint32_t number;      /* tag number (supports high-tag-number form)  */
} ber_tag;

/* Encode an identifier octet (or multi-octet high-tag-number form). */
dlms_status ber_write_tag(dlms_writer *w, ber_tag tag);

/* Decode an identifier. Handles high-tag-number form (number >= 31). */
dlms_status ber_read_tag(dlms_reader *r, ber_tag *tag);

/* Read one full TLV. On success, tag, val and val_len describe the element,
 * the reader is advanced past the value, and *indefinite reports whether the
 * length used the indefinite form (in which case *val_len spans up to the
 * matching end-of-contents, which is NOT consumed -- caller handles nesting).
 * For definite lengths *indefinite is false and val points into r->buf. */
dlms_status ber_read_tlv(dlms_reader *r, ber_tag *tag,
                         const uint8_t **val, size_t *val_len,
                         bool *indefinite);

/* Convenience: begin a constructed element. Because the length is not known
 * until children are written, we reserve a length slot (long form, fixed
 * width) and patch it in ber_end(). `hdr` must be passed unchanged to
 * ber_end(). width is the number of length octets reserved (1..4). */
typedef struct { size_t len_pos; uint8_t width; } ber_frame;

dlms_status ber_begin(dlms_writer *w, ber_tag tag, uint8_t width, ber_frame *hdr);
dlms_status ber_end(dlms_writer *w, const ber_frame *hdr);

/* Write a primitive TLV in one call. */
dlms_status ber_write_primitive(dlms_writer *w, ber_tag tag,
                                const uint8_t *val, size_t val_len);

/* =====================================================================
 *  A-XDR -- the COSEM `Data` type
 * ===================================================================== */

/* COSEM Data CHOICE tags (per DLMS Blue Book). Cross-check against your copy;
 * these are the stable, widely-implemented values. Gaps (7,8,11,14) are
 * deprecated/unused. */
typedef enum {
    AXDR_NULL          = 0,
    AXDR_ARRAY         = 1,
    AXDR_STRUCTURE     = 2,
    AXDR_BOOLEAN       = 3,
    AXDR_BIT_STRING    = 4,
    AXDR_DOUBLE_LONG   = 5,   /* int32                                     */
    AXDR_DOUBLE_LONG_U = 6,   /* uint32                                    */
    AXDR_OCTET_STRING  = 9,
    AXDR_VISIBLE_STRING= 10,
    AXDR_UTF8_STRING   = 12,
    AXDR_BCD           = 13,  /* int8 (BCD)                                */
    AXDR_INTEGER       = 15,  /* int8                                      */
    AXDR_LONG          = 16,  /* int16                                     */
    AXDR_UNSIGNED      = 17,  /* uint8                                     */
    AXDR_LONG_U        = 18,  /* uint16                                    */
    AXDR_COMPACT_ARRAY = 19,  /* not implemented (see notes)               */
    AXDR_LONG64        = 20,  /* int64                                     */
    AXDR_LONG64_U      = 21,  /* uint64                                    */
    AXDR_ENUM          = 22,  /* uint8                                     */
    AXDR_FLOAT32       = 23,  /* IEEE-754 binary32                         */
    AXDR_FLOAT64       = 24,  /* IEEE-754 binary64                         */
    AXDR_DATE_TIME     = 25,  /* fixed 12 octets, no length prefix         */
    AXDR_DATE          = 26,  /* fixed 5  octets, no length prefix         */
    AXDR_TIME          = 27   /* fixed 4  octets, no length prefix         */
} axdr_type;

/* Fixed sizes for the date/time family. */
enum { AXDR_DATE_TIME_LEN = 12, AXDR_DATE_LEN = 5, AXDR_TIME_LEN = 4 };

/* In-memory COSEM Data value. For encoding, the caller fills this and any
 * referenced memory (bytes / list items) stays owned by the caller. For
 * scalar decode, primitive fields are filled; for octet/string/date types,
 * `bytes` points INTO the source buffer (zero-copy). Containers are decoded
 * with the pull API (see axdr_decode_data) rather than into this tree. */
typedef struct axdr_data axdr_data;
struct axdr_data {
    axdr_type type;
    union {
        bool     boolean;
        int8_t   i8;   uint8_t  u8;
        int16_t  i16;  uint16_t u16;
        int32_t  i32;  uint32_t u32;
        int64_t  i64;  uint64_t u64;
        float    f32;  double   f64;
        /* octet/visible/utf8-string, date/time: ptr+len of raw octets.
         * bit-string: ptr = octets, len = NUMBER OF BITS (octets = (len+7)/8). */
        struct { const uint8_t *ptr; size_t len; } bytes;
        /* array/structure (encode path): items + count. */
        struct { const axdr_data *items; size_t count; } list;
    } u;
};

/* --- A-XDR length (identical short/long form to BER, no indefinite) ------ */
/* For bit-string the length is a bit count; for array/structure an element
 * count; for octet strings an octet count. Use dlms_write_len/dlms_read_len. */

/* --- Encode ------------------------------------------------------------- */
/* Recursively encode a COSEM Data tree. */
dlms_status axdr_encode_data(dlms_writer *w, const axdr_data *d);

/* --- Decode (pull API) -------------------------------------------------- */
/* Header returned when pulling one Data value. For scalars and string/date
 * types, `value` is fully populated and the reader is past the element. For
 * AXDR_ARRAY / AXDR_STRUCTURE, `count` holds the element count and the reader
 * is positioned at the first child -- call axdr_decode_data() `count` times
 * (recursing) to walk them. */
typedef struct {
    axdr_type type;
    size_t    count;  /* meaningful for ARRAY / STRUCTURE only */
    axdr_data value;  /* meaningful for scalars / strings / dates */
    /* meaningful for AXDR_COMPACT_ARRAY only: raw spans into the source buffer
     * for the TypeDescription and the packed array-contents. Walk them with
     * axdr_compact_array_walk(). */
    struct { const uint8_t *desc; size_t desc_len;
             const uint8_t *contents; size_t contents_len; } compact;
} axdr_header;

dlms_status axdr_decode_data(dlms_reader *r, axdr_header *out);

/* Visitor-style full walk for convenience (used by the demo). `depth` starts
 * at 0. Return non-OK from the callback to abort. Compact-arrays are expanded:
 * the visitor sees the AXDR_COMPACT_ARRAY node, then one node per element
 * (structure/scalar) at depth+1, reconstructed from the TypeDescription. */
typedef dlms_status (*axdr_visit_fn)(const axdr_header *h, int depth, void *ctx);
dlms_status axdr_walk(dlms_reader *r, axdr_visit_fn fn, void *ctx);

/* --- compact-array (Data tag 19) --------------------------------------- */
/* Encode `arr` (must be AXDR_ARRAY, count >= 1, all items the same shape) as a
 * compact-array: TypeDescription derived from items[0], then values packed
 * tagless. Supports scalar, octet/string, nested structure and nested array
 * element types. array-contents length uses minimal A-XDR form.
 * Note: axdr_encode_data() does NOT produce compact-arrays; use this. */
dlms_status axdr_encode_compact_array(dlms_writer *w, const axdr_data *arr);

/* Walk a decoded compact-array (h->type == AXDR_COMPACT_ARRAY), emitting each
 * element to the visitor (base depth 0). */
dlms_status axdr_compact_array_walk(const axdr_header *h, axdr_visit_fn fn, void *ctx);

/* =====================================================================
 *  APDU dispatch: which encoding applies to a received frame.
 * ===================================================================== */
typedef enum { DLMS_ENC_BER_ACSE, DLMS_ENC_AXDR_XDLMS, DLMS_ENC_UNKNOWN } dlms_encoding;

/* Decide the encoding from the leading APDU tag. This is the correct way to
 * "detect" BER vs A-XDR: by protocol-defined tag, never by heuristics. */
dlms_encoding dlms_encoding_for_apdu(uint8_t first_tag);

#ifdef __cplusplus
}
#endif
#endif /* DLMS_CODEC_H */
