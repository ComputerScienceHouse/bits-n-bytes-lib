/* ESP-IDF port: the "port" is the UART number. */
#include "bnb_uart.h"
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"

#define RX_BUFFER_SIZE 1024
#define TX_BUFFER_SIZE 1024

int bnb_esp32_uart_open(int uart_num, int tx_pin, int rx_pin, int baud) {
    uart_config_t cfg = {
        .baud_rate = baud,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };
    if (uart_driver_install(uart_num, RX_BUFFER_SIZE, TX_BUFFER_SIZE, 0, NULL, 0) != ESP_OK) return -1;
    if (uart_param_config(uart_num, &cfg) != ESP_OK ||
        uart_set_pin(uart_num, tx_pin, rx_pin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE) != ESP_OK) {
        uart_driver_delete(uart_num);
        return -1;
    }
    return uart_num;
}

int bnb_port_write(int port, const uint8_t *data, size_t len) {
    return uart_write_bytes(port, data, len);
}

int bnb_port_read_byte(int port, uint8_t *byte, int timeout_ms) {
    return uart_read_bytes(port, byte, 1, pdMS_TO_TICKS(timeout_ms));  /* 1, 0, or -1 */
}
