#ifndef _UART_H_
#define _UART_H_

#define UART_WRITE_BLOCKING(X) uart_write_blocking(X, sizeof(X) - 1)

#define UART_WRITE_STR(X) uart_write(X, sizeof(X) - 1)

#define UART_WRITE_CMD2(X, P1, L1, P2, L2) uart_write_cmd2(X, sizeof(X) - 1, (P1), (L1), (P2), (L2))

uint16_t uart_read(char *buffer, uint16_t length);
void uart_write_blocking(const char *buffer, uint16_t length);
void uart_write(const char *buffer, uint16_t length);
void uart_write_cmd2(const char *cmd, uint16_t length, void *p1, uint16_t p1_len, void *p2, uint16_t p2_len);
void uart_setup();

#endif
