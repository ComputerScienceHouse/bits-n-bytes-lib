/* Linux port: the "port" is a file descriptor. Close it with close(). */
#define _DEFAULT_SOURCE
#include "bnb_uart.h"
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <termios.h>
#include <unistd.h>

static speed_t baud_to_speed(int baud) {
    switch (baud) {
    case 9600:   return B9600;
    case 19200:  return B19200;
    case 38400:  return B38400;
    case 57600:  return B57600;
    case 115200: return B115200;
    case 230400: return B230400;
    default:     return B0;  /*B0 is a hang up. add more here if you need them */
    }
}

int bnb_linux_uart_open(const char *device, int baud) {
    speed_t speed = baud_to_speed(baud);
    if (speed == B0) return -1;

    int fd = open(device, O_RDWR | O_NOCTTY | O_CLOEXEC);
    if (fd < 0) return -1;

    struct termios tio;
    if (tcgetattr(fd, &tio) != 0) { close(fd); return -1; }
    cfmakeraw(&tio);                       /* raw 8N1: the tty must not touch our bytes */
    tio.c_cflag |= (CLOCAL | CREAD);
    cfsetispeed(&tio, speed);
    cfsetospeed(&tio, speed);
    if (tcsetattr(fd, TCSANOW, &tio) != 0) { close(fd); return -1; }
    return fd;
}

int bnb_port_write(int port, const uint8_t *data, size_t len) {
    size_t done = 0;
    while (done < len) {
        ssize_t n = write(port, data + done, len - done);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        done += (size_t)n;
    }
    return (int)done;
}

int bnb_port_read_byte(int port, uint8_t *byte, int timeout_ms) {
    struct pollfd p = { .fd = port, .events = POLLIN };
    int r;
    do { r = poll(&p, 1, timeout_ms); } while (r < 0 && errno == EINTR);
    if (r < 0)  return -1;
    if (r == 0) return 0;
    return (read(port, byte, 1) == 1) ? 1 : -1;
}
