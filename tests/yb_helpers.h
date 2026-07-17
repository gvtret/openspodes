/**
 * yb_helpers.h — Shared helpers for Yellow Book conformance tests.
 *
 * Provides standard loopback setup and environment initialization
 * to reduce boilerplate across YB test files.
 */
#ifndef YB_HELPERS_H
#define YB_HELPERS_H

#include "mock_transport.h"
#include "mock_crypto.h"
#include "../src/openspodes.h"
#include "../src/client/client.h"
#include "../src/server/server.h"
#include "../src/server/dispatcher.h"
#include "../src/ic/data.h"
#include "../src/security/security.h"

/** Standard OBIS code used by default test Data object */
#define YB_TEST_OBIS ((osp_obis_t){0, 0, 1, 0, 0, 255})

/**
 * Setup loopback transport pair with auto-processing server.
 * After calling this, osp_client_connect() will automatically
 * trigger osp_server_accept() for each client send.
 */
void yb_setup_loopback(mock_transport_pair_t *pair, osp_server_t *server);

/**
 * Setup a standard low-security server with a single Data object at YB_TEST_OBIS.
 * Does NOT create the client — use yb_make_pair() for that.
 */
void yb_setup_server(osp_server_t *server, mock_transport_pair_t *pair, uint32_t initial_value);

/**
 * Create a standard low-security client+server pair with loopback transport.
 * Server has a single Data object at YB_TEST_OBIS.
 */
void yb_make_pair(mock_transport_pair_t *pair, osp_server_t *server, osp_client_t *client, uint32_t initial_value);

#endif /* YB_HELPERS_H */
