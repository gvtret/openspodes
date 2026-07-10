#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "../src/openspodes.h"
#include "../src/codec/codec.h"

static void ber_backpatch_length(osp_buf_t *buf, uint32_t len_pos) {
	uint32_t content_len = buf->wr - len_pos - 1;
	if (content_len < 0x80) {
		buf->buf[len_pos] = (uint8_t)content_len;
	} else {
		memmove(&buf->buf[len_pos + 2], &buf->buf[len_pos + 1], content_len);
		buf->buf[len_pos] = 0x81;
		buf->buf[len_pos + 1] = (uint8_t)content_len;
		buf->wr++;
	}
}

int main(void) {
	uint8_t buf[64];
	osp_buf_t b;
	osp_buf_init(&b, buf, sizeof(buf));

	osp_ber_write_tag(&b, 2, true, 1); /* [1] EXPLICIT */

	uint32_t len_pos = b.wr;
	osp_ber_write_length(&b, 0); /* placeholder */

	uint8_t prefix[] = {0x60, 0x85, 0x74, 0x05, 0x08, 0x01};
	osp_ber_write_tag(&b, 0, false, 6);
	osp_ber_write_length(&b, 7);
	for (int i = 0; i < 6; i++) {
		osp_axdr_write_u8(&b, prefix[i]);
	}
	osp_axdr_write_u8(&b, 0x01);

	ber_backpatch_length(&b, len_pos);

	/* OID block is 9 bytes; [1] EXPLICIT content length must match */
	assert(buf[len_pos] == 9);

	osp_buf_t r;
	osp_buf_init(&r, buf, b.wr);
	r.wr = b.wr;
	osp_ber_tag_t tag;
	uint32_t len;
	assert(osp_ber_read_tag(&r, &tag) == OSP_OK);
	assert(tag.tag_class == 2);
	assert(tag.tag_constructed);
	assert(tag.tag_number == 1);
	assert(osp_ber_read_length(&r, &len) == OSP_OK);
	assert(len == 9);
	assert(osp_buf_unread(&r) == 9);

	printf("BER [1] EXPLICIT length backpatch OK (len=%u, wr=%u)\n", len, b.wr);
	return 0;
}
