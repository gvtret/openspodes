#include "concentrator.h"
#include "../transport/transport.h"
#include <string.h>

static bool meter_id_eq(const uint8_t *a, uint8_t a_len, const uint8_t *b, uint8_t b_len) {
	return a_len == b_len && memcmp(a, b, a_len) == 0;
}

void osp_spodus_concentrator_init(osp_spodus_concentrator_t *c) {
	if (!c) {
		return;
	}
	memset(c, 0, sizeof(*c));
	osp_spodus_registry_init(&c->registry);
	osp_spodus_direct_table_init(&c->direct);
	osp_spodus_channel_list_init(&c->channels);
	osp_spodus_discovered_list_init(&c->discovered);
	osp_spodus_access_policies_init(&c->access_policies);
}

osp_err_t osp_spodus_concentrator_attach_downstream(osp_spodus_concentrator_t *c, const uint8_t *meter_id, uint8_t meter_id_len,
                                                    osp_transport_t *transport, osp_framing_type_t framing) {
	if (!c || !meter_id || meter_id_len == 0 || !transport) {
		return OSP_ERR_INVALID;
	}
	for (uint8_t i = 0; i < c->downstream_count; i++) {
		if (meter_id_eq(c->downstream[i].meter_id, c->downstream[i].meter_id_len, meter_id, meter_id_len)) {
			c->downstream[i].transport = transport;
			c->downstream[i].framing = framing;
			c->downstream[i].connected = false;
			return osp_client_init(&c->downstream[i].client, transport, framing);
		}
	}
	if (c->downstream_count >= OSP_SPODUS_MAX_METERS) {
		return OSP_ERR_NOMEM;
	}
	osp_spodus_downstream_t *link = &c->downstream[c->downstream_count++];
	link->meter_id_len = meter_id_len;
	memcpy(link->meter_id, meter_id, meter_id_len);
	link->transport = transport;
	link->framing = framing;
	link->connected = false;
	return osp_client_init(&link->client, transport, framing);
}

osp_spodus_downstream_t *osp_spodus_concentrator_downstream(osp_spodus_concentrator_t *c, const uint8_t *meter_id, uint8_t meter_id_len) {
	if (!c || !meter_id) {
		return NULL;
	}
	for (uint8_t i = 0; i < c->downstream_count; i++) {
		if (meter_id_eq(c->downstream[i].meter_id, c->downstream[i].meter_id_len, meter_id, meter_id_len)) {
			return &c->downstream[i];
		}
	}
	return NULL;
}

osp_err_t osp_spodus_concentrator_connect_downstream(osp_spodus_concentrator_t *c, const uint8_t *meter_id, uint8_t meter_id_len,
                                                     uint32_t timeout_ms) {
	osp_spodus_downstream_t *link = osp_spodus_concentrator_downstream(c, meter_id, meter_id_len);
	if (!link) {
		return OSP_ERR_NOT_FOUND;
	}
	osp_err_t r = osp_client_connect(&link->client, timeout_ms);
	link->connected = (r == OSP_OK);
	return r;
}

uint32_t osp_spodus_poll_meter(osp_client_t *client, osp_spodus_meter_registry_t *registry, const uint8_t *meter_id, uint8_t meter_id_len,
                               const osp_spodus_attr_ref_t *attributes, uint8_t count) {
	if (!client || !registry || !meter_id || !attributes || count == 0) {
		return 0;
	}
	uint32_t read = 0;
	for (uint8_t i = 0; i < count; i++) {
		osp_value_t result;
		if (osp_client_get(client, attributes[i].class_id, &attributes[i].obis, attributes[i].attribute_id, &result) == OSP_OK) {
			if (osp_spodus_registry_store(registry, meter_id, meter_id_len, attributes[i].obis, attributes[i].attribute_id, &result) == OSP_OK) {
				read++;
			}
		}
	}
	return read;
}

osp_err_t osp_spodus_proxy_forward(osp_spodus_concentrator_t *c, uint16_t direct_id, const uint8_t *request, uint32_t request_len,
                                   uint8_t *response, uint32_t response_size, uint32_t *response_len, uint32_t timeout_ms) {
	if (!c || !request || request_len == 0 || !response || !response_len) {
		return OSP_ERR_INVALID;
	}
	const osp_spodus_direct_channel_t *ch = osp_spodus_direct_table_find(&c->direct, direct_id);
	if (!ch) {
		return OSP_ERR_NOT_FOUND;
	}
	osp_spodus_downstream_t *link = osp_spodus_concentrator_downstream(c, ch->meter_id, ch->meter_id_len);
	if (!link || !link->transport) {
		return OSP_ERR_IO;
	}
	osp_err_t r = osp_transport_send_apdu(link->transport, link->framing, request, request_len);
	if (r != OSP_OK) {
		return r;
	}
	return osp_transport_recv_apdu(link->transport, link->framing, response, response_size, response_len, timeout_ms);
}
