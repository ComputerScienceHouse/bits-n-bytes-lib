#ifndef BNB_UART_H
#define BNB_UART_H

/*
 * bnb_uart: send and receive framed, CRC-checked messages over a UART.
 * Plain C, no dependencies beyond libc. Works the same on ESP32 and Linux.
 *
 * Frame on the wire:
 *
 *   [0x02] [LEN_HI] [LEN_LO] [payload ... LEN bytes] [CRC_HI] [CRC_LO] [0x03]
 *    start  \_____ length __/                         \__ CRC __/      end
 *
 * CRC is CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF) over the two length
 * bytes plus the payload.
 *
 * ---------------------------------------------------------------------------
 * EXTENDING
 *
 * New platform (e.g. a TCP socket, a USB serial port):
 *   Implement the two bnb_port_* functions below, plus an open function that
 *   returns an int "port" (a UART number, a file descriptor, ...). Nothing
 *   in bnb_uart.c needs to change.
 *
 * Message types / extra header fields:
 *   Put them at the start of the payload, e.g. payload[0] = message type.
 *   The CRC already covers them.
 *
 * Different limits or markers:
 *   Change the #defines below. Both sides must agree.
 * ---------------------------------------------------------------------------
 */

#include <stdint.h>
#include <stddef.h>

#define BNB_START       0x02
#define BNB_END         0x03
#define BNB_MAX_PAYLOAD 512
#define BNB_CRC_INIT    0xFFFF

/* Return codes (0 = success, negative = failure) */
enum {
    BNB_OK         =  0,
    BNB_TIMEOUT    = -1,  /* no data arrived in time */
    BNB_ERR_IO     = -2,  /* port read/write failed */
    BNB_ERR_FRAME  = -3,  /* bad length or missing end byte */
    BNB_ERR_CRC    = -4,  /* checksum mismatch */
    BNB_ERR_SIZE   = -5,  /* payload too big to send, or buffer too small to receive into */
};

/* Send one message. */
int bnb_send(int port, const void *payload, size_t len);

/*
 * Wait for one message. The payload is copied into buf and NUL-terminated
 * (so buf_size must be at least payload length + 1), ready for cJSON_Parse.
 *
 * timeout_ms is how long to wait for each incoming byte.
 *
 * On any error other than BNB_ERR_IO, just call again: the next call skips
 * ahead to the next start byte, so the stream resyncs by itself.
 */
int bnb_receive(int port, uint8_t *buf, size_t buf_size, size_t *len, int timeout_ms);

/* CRC helper. Start with BNB_CRC_INIT; pass the result back in to continue. */
uint16_t bnb_crc16(uint16_t crc, const void *data, size_t len);

/* ---- Port interface: implement these for each platform ---- */

/* Write all len bytes. Return bytes written, or -1 on error. */
int bnb_port_write(int port, const uint8_t *data, size_t len);

/* Read one byte, waiting up to timeout_ms. Return 1 = got a byte, 0 = timeout, -1 = error. */
int bnb_port_read_byte(int port, uint8_t *byte, int timeout_ms);

/* ---- Opening a port (platform specific; each returns a port or -1) ---- */

int bnb_esp32_uart_open(int uart_num, int tx_pin, int rx_pin, int baud);  /* port_esp32.c */
int bnb_linux_uart_open(const char *device, int baud);                    /* port_linux.c */

#endif
