#include "bnb_uart.h"
#include <string.h>

uint16_t bnb_crc16(uint16_t crc, const void *data, size_t len) {
    const uint8_t *p = (const uint8_t *)data;
    while (len--) {
        crc ^= (uint16_t)(*p++ << 8);
        for (int i = 0; i < 8; i++) {
            crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021) : (uint16_t)(crc << 1);
        }
    }
    return crc;
}

int bnb_send(int port, const void *payload, size_t len) {
    if (len > BNB_MAX_PAYLOAD) return BNB_ERR_SIZE;

    uint8_t frame[BNB_MAX_PAYLOAD + 6];
    frame[0] = BNB_START;
    frame[1] = (uint8_t)(len >> 8);
    frame[2] = (uint8_t)(len & 0xFF);
    if (len > 0) memcpy(&frame[3], payload, len);

    uint16_t crc = bnb_crc16(BNB_CRC_INIT, &frame[1], len + 2);  /* length + payload */
    frame[3 + len] = (uint8_t)(crc >> 8);
    frame[4 + len] = (uint8_t)(crc & 0xFF);
    frame[5 + len] = BNB_END;

    /* One write for the whole frame so it can't be interleaved with another. */
    size_t total = len + 6;
    return (bnb_port_write(port, frame, total) == (int)total) ? BNB_OK : BNB_ERR_IO;
}

/*
 * Add one received byte to the frame being assembled.
 * Returns 1 = frame complete, 0 = need more bytes, <0 = bad frame (dropped).
 */
static int feed(bnb_rx_t *rx, uint8_t b) {
    if (rx->count == 0 && b != BNB_START) return 0;  /* skipping noise until a start byte */
    rx->buf[rx->count++] = b;

    if (rx->count < 3) return 0;                     /* still reading the length */
    size_t n = ((size_t)rx->buf[1] << 8) | rx->buf[2];
    if (n > BNB_MAX_PAYLOAD) {
        rx->count = 0;
        return BNB_ERR_FRAME;
    }
    if (rx->count < n + 6) return 0;                 /* still reading payload / CRC / end */

    /* Whole frame is here: [start][len 2][payload n][crc 2][end] */
    rx->count = 0;
    uint16_t expected = bnb_crc16(BNB_CRC_INIT, &rx->buf[1], n + 2);
    uint16_t received = (uint16_t)((rx->buf[3 + n] << 8) | rx->buf[4 + n]);
    if (received != expected)     return BNB_ERR_CRC;
    if (rx->buf[5 + n] != BNB_END) return BNB_ERR_FRAME;

    rx->buf[3 + n] = '\0';                           /* safe: CRC byte already checked */
    rx->payload = &rx->buf[3];
    rx->len = n;
    return 1;
}

int bnb_receive(int port, bnb_rx_t *rx, int timeout_ms) {
    for (;;) {
        uint8_t b;
        int r = bnb_port_read_byte(port, &b, timeout_ms);
        if (r == 0) return BNB_NO_FRAME;
        if (r < 0)  return BNB_ERR_IO;

        int result = feed(rx, b);
        if (result == 1) return BNB_OK;
        if (result < 0)  return result;
    }
}
