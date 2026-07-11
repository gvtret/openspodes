/**
 * serial_transport.h — Serial (UART/RS-485) transport for OpenSPODES
 *
 * Implements osp_transport_t for Linux serial ports.
 * Supports standard baud rates, RS-485 direction control (RTS).
 *
 * Usage:
 *   osp_transport_t transport;
 *   linux_serial_ctx_t ctx;
 *   linux_serial_transport_init(&transport, &ctx, "/dev/ttyUSB0", B9600);
 *   // Use transport with HDLC session or wrapper framing
 */

#ifndef OSP_LINUX_SERIAL_H
#define OSP_LINUX_SERIAL_H

#include "../src/openspodes.h"
#include "../src/transport/transport.h"
#include <termios.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Serial port configuration */
typedef struct {
	int fd;
	int rts_fd;           /* RTS GPIO fd for RS-485 direction control, -1 = none */
	uint32_t timeout_ms;  /* Default read timeout */
} linux_serial_ctx_t;

/* Initialize serial transport on a port */
osp_err_t linux_serial_transport_init(osp_transport_t *t, linux_serial_ctx_t *ctx,
                                       const char *port, speed_t baud);

/* Set RS-485 RTS GPIO pin (for direction control) */
osp_err_t linux_serial_set_rts(linux_serial_ctx_t *ctx, const char *rts_gpio);

#ifdef __cplusplus
}
#endif

#endif /* OSP_LINUX_SERIAL_H */
