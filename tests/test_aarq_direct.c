#include <stdio.h>
#include <string.h>
#include "../src/openspodes.h"
#include "../src/service/service.h"

int main(void) {
	/* Encode AARQ */
	osp_aarq_t aarq;
	memset(&aarq, 0, sizeof(aarq));
	aarq.application_context = 1; /* LN */
	aarq.mechanism = 1;           /* LLS */
	aarq.calling_auth_value_len = 8;
	for (int i = 0; i < 8; i++)
		aarq.calling_auth_value[i] = 0x30 + i;
	aarq.user_info[0] = 0x00;
	aarq.user_info[1] = 0x01;
	aarq.user_info_len = 2;

	uint8_t buf[512];
	osp_buf_t w;
	osp_buf_init(&w, buf, sizeof(buf));
	int r = osp_aarq_encode(&aarq, &w);
	printf("encode: r=%d len=%u\n", r, w.wr);
	printf("bytes: ");
	for (uint32_t i = 0; i < w.wr; i++)
		printf("%02X ", buf[i]);
	printf("\n");

	/* Decode AARQ */
	osp_aarq_t decoded;
	osp_buf_t rd;
	osp_buf_init(&rd, buf, w.wr);
	rd.wr = w.wr;
	r = osp_aarq_decode(&rd, &decoded);
	printf("decode: r=%d\n", r);
	if (r == 0) {
		printf(
		    "app_ctx=%d mechanism=%d auth_len=%d user_info_len=%d\n", decoded.application_context, decoded.mechanism, decoded.calling_auth_value_len,
		    decoded.user_info_len
		);
		printf("auth: ");
		for (int i = 0; i < decoded.calling_auth_value_len; i++)
			printf("%02X ", decoded.calling_auth_value[i]);
		printf("\n");
	}

	return r;
}
