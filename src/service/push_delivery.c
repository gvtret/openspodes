#include "push_delivery.h"

static osp_push_delivery_fn g_push_delivery_fn;
static void *g_push_delivery_ctx;

void osp_push_set_delivery_fn(osp_push_delivery_fn fn, void *ctx) {
	g_push_delivery_fn = fn;
	g_push_delivery_ctx = ctx;
}

osp_err_t osp_push_schedule_delivery(const osp_push_delivery_request_t *req) {
	if (!req || !g_push_delivery_fn) {
		return OSP_ERR_NOT_FOUND;
	}
	return g_push_delivery_fn(req, g_push_delivery_ctx);
}
