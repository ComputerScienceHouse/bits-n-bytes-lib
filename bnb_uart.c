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

/* Read exactly n bytes. */
static int read_exact(int port, uint8_t *dst, size_t n, int timeout_ms) {
    for (size_t i = 0; i < n; i++) {
        int r = bnb_port_read_byte(port, &dst[i], timeout_ms);
        if (r == 0) return BNB_TIMEOUT;
        if (r < 0)  return BNB_ERR_IO;
    }
    return BNB_OK;
}

int bnb_receive(int port, uint8_t *buf, size_t buf_size, size_t *len, int timeout_ms) {
    int err;
    uint8_t b;

    /* 1. Skip bytes until the start marker */
    do {
        if ((err = read_exact(port, &b, 1, timeout_ms)) != BNB_OK) return err;
    } while (b != BNB_START);

    /* 2. Length */
    uint8_t length_bytes[2];
    if ((err = read_exact(port, length_bytes, 2, timeout_ms)) != BNB_OK) return err;
    size_t n = ((size_t)length_bytes[0] << 8) | length_bytes[1];
    if (n > BNB_MAX_PAYLOAD) return BNB_ERR_FRAME;
    if (n + 1 > buf_size)    return BNB_ERR_SIZE;

    /* 3. Payload, CRC, end marker */
    if ((err = read_exact(port, buf, n, timeout_ms)) != BNB_OK) return err;
    uint8_t tail[3];
    if ((err = read_exact(port, tail, 3, timeout_ms)) != BNB_OK) return err;

    /* 4. Validate */
    uint16_t expected = bnb_crc16(BNB_CRC_INIT, length_bytes, 2);
    expected = bnb_crc16(expected, buf, n);
    uint16_t received = (uint16_t)((tail[0] << 8) | tail[1]);
    if (received != expected) return BNB_ERR_CRC;
    if (tail[2] != BNB_END)   return BNB_ERR_FRAME;

    buf[n] = '\0';
    if (len) *len = n;
    return BNB_OK;
}
