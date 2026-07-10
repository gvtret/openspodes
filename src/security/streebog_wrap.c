#include "gost_crypto.h"
#include "streebog/gost3411-2012-core.h"
#include <stdlib.h>
#include <string.h>

int osp_gost_streebog256(const uint8_t *input, uint32_t len, uint8_t output[32]) {
	if (!input || !output) {
		return -1;
	}
	GOST34112012Context ctx;
	memset(&ctx, 0, sizeof(ctx));
	GOST34112012Init(&ctx, 256);
	GOST34112012Update(&ctx, input, len);
	GOST34112012Final(&ctx, output);
	GOST34112012Cleanup(&ctx);
	return 0;
}
