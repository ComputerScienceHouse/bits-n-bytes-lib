/* Linux self-test: uses a pseudo-terminal pair as a stand-in for a serial cable. */
#define _XOPEN_SOURCE 600
#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "bnb_uart.h"

/* Poll without blocking, the way a main loop would, until something other than NO_FRAME happens. */
static int poll_until_done(int port, bnb_rx_t *rx) {
    for (int i = 0; i < 1000; i++) {
        int r = bnb_receive(port, rx, 0);
        if (r != BNB_NO_FRAME) return r;
        usleep(1000);
    }
    return BNB_NO_FRAME;
}

static long elapsed_ms(struct timespec t0) {
    struct timespec t1;
    clock_gettime(CLOCK_MONOTONIC, &t1);
    return (t1.tv_sec - t0.tv_sec) * 1000 + (t1.tv_nsec - t0.tv_nsec) / 1000000;
}

int main(void) {
    assert(bnb_crc16(BNB_CRC_INIT, "123456789", 9) == 0x29B1);   /* standard check value */

    int a = posix_openpt(O_RDWR | O_NOCTTY);                     /* one end of the "cable" */
    assert(a >= 0 && grantpt(a) == 0 && unlockpt(a) == 0);
    int b = bnb_linux_uart_open(ptsname(a), 115200);             /* the other end */
    assert(b >= 0);

    bnb_rx_t rx_a = {0}, rx_b = {0};

    /* non-blocking with nothing to read returns right away */
    struct timespec t0;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    assert(bnb_receive(b, &rx_b, 0) == BNB_NO_FRAME);
    assert(elapsed_ms(t0) < 5);

    /* basic delivery, both directions (non-blocking polling) */
    assert(bnb_send(a, "hello", 5) == BNB_OK);
    assert(poll_until_done(b, &rx_b) == BNB_OK);
    assert(rx_b.len == 5 && strcmp((const char *)rx_b.payload, "hello") == 0);

    assert(bnb_send(b, "{\"doors\":true}\n", 15) == BNB_OK);     /* includes a 0x0A byte */
    assert(poll_until_done(a, &rx_a) == BNB_OK);
    assert(rx_a.len == 15 && rx_a.payload[14] == '\n');

    /* a frame arriving in two pieces: progress is kept between calls */
    uint8_t frame[16] = { BNB_START, 0x00, 0x05, 's', 'p', 'l', 'i', 't' };
    uint16_t crc = bnb_crc16(BNB_CRC_INIT, &frame[1], 7);
    frame[8] = (uint8_t)(crc >> 8); frame[9] = (uint8_t)(crc & 0xFF); frame[10] = BNB_END;
    assert(write(a, frame, 4) == 4);
    for (int i = 0; i < 5; i++) { assert(bnb_receive(b, &rx_b, 0) == BNB_NO_FRAME); usleep(2000); }
    assert(write(a, frame + 4, 7) == 7);
    assert(poll_until_done(b, &rx_b) == BNB_OK);
    assert(strcmp((const char *)rx_b.payload, "split") == 0);

    /* two frames back to back: each call returns one */
    assert(bnb_send(a, "one", 3) == BNB_OK);
    assert(bnb_send(a, "two", 3) == BNB_OK);
    assert(poll_until_done(b, &rx_b) == BNB_OK && strcmp((const char *)rx_b.payload, "one") == 0);
    assert(poll_until_done(b, &rx_b) == BNB_OK && strcmp((const char *)rx_b.payload, "two") == 0);

    /* noise + corrupted frame, then a good frame: error is reported, then it recovers */
    const uint8_t junk_and_bad[] = { 0x55, 0xAA, BNB_START, 0x00, 0x01, 'A', 0x12, 0x34, BNB_END };
    assert(write(a, junk_and_bad, sizeof junk_and_bad) == (ssize_t)sizeof junk_and_bad);
    assert(bnb_send(a, "ok", 2) == BNB_OK);
    assert(poll_until_done(b, &rx_b) == BNB_ERR_CRC);
    assert(poll_until_done(b, &rx_b) == BNB_OK && strcmp((const char *)rx_b.payload, "ok") == 0);

    /* blocking mode still works: timeout > 0 waits */
    assert(bnb_send(a, "wait", 4) == BNB_OK);
    assert(bnb_receive(b, &rx_b, 100) == BNB_OK && strcmp((const char *)rx_b.payload, "wait") == 0);
    clock_gettime(CLOCK_MONOTONIC, &t0);
    assert(bnb_receive(b, &rx_b, 50) == BNB_NO_FRAME);
    assert(elapsed_ms(t0) >= 40);

    /* send size limit */
    uint8_t big[BNB_MAX_PAYLOAD + 1] = {0};
    assert(bnb_send(a, big, sizeof big) == BNB_ERR_SIZE);

    puts("all tests passed");
    close(a); close(b);
    return 0;
}
