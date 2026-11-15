/* Linux self-test: uses a pseudo-terminal pair as a stand-in for a serial cable. */
#define _XOPEN_SOURCE 600
#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "bnb_uart.h"

int main(void) {
    assert(bnb_crc16(BNB_CRC_INIT, "123456789", 9) == 0x29B1);   /* standard check value */

    int a = posix_openpt(O_RDWR | O_NOCTTY);                     /* one end of the "cable" */
    assert(a >= 0 && grantpt(a) == 0 && unlockpt(a) == 0);
    int b = bnb_linux_uart_open(ptsname(a), 115200);             /* the other end */
    assert(b >= 0);

    uint8_t buf[64];
    size_t len;

    /* both directions */
    assert(bnb_send(a, "hello", 5) == BNB_OK);
    assert(bnb_receive(b, buf, sizeof buf, &len, 100) == BNB_OK);
    assert(len == 5 && strcmp((char *)buf, "hello") == 0);

    assert(bnb_send(b, "{\"doors\":true}\n", 15) == BNB_OK);     /* includes a 0x0A byte */
    assert(bnb_receive(a, buf, sizeof buf, &len, 100) == BNB_OK);
    assert(len == 15 && buf[14] == '\n');

    /* nothing to read */
    assert(bnb_receive(b, buf, sizeof buf, &len, 20) == BNB_TIMEOUT);

    /* noise + corrupted frame, then a good frame: first call reports the error, second recovers */
    const uint8_t junk_and_bad[] = { 0x55, 0xAA, BNB_START, 0x00, 0x01, 'A', 0x12, 0x34, BNB_END };
    assert(write(a, junk_and_bad, sizeof junk_and_bad) == (ssize_t)sizeof junk_and_bad);
    assert(bnb_send(a, "ok", 2) == BNB_OK);
    assert(bnb_receive(b, buf, sizeof buf, &len, 100) == BNB_ERR_CRC);
    assert(bnb_receive(b, buf, sizeof buf, &len, 100) == BNB_OK);
    assert(strcmp((char *)buf, "ok") == 0);

    /* size limits */
    assert(bnb_send(a, buf, BNB_MAX_PAYLOAD + 1) == BNB_ERR_SIZE);
    assert(bnb_send(a, "too long for buf", 16) == BNB_OK);
    uint8_t tiny[8];
    assert(bnb_receive(b, tiny, sizeof tiny, &len, 100) == BNB_ERR_SIZE);

    puts("all tests passed");
    close(a); close(b);
    return 0;
}
