#include "xc.h"

#include "led.h"

static char uart_rx_buffer[256];
static volatile uint8_t uart_rx_write;
static volatile uint8_t uart_rx_read;

void __attribute__((interrupt, no_auto_psv)) _U1RXInterrupt() {
  while(U1STAbits.URXDA) {
    uart_rx_buffer[uart_rx_write++] = U1RXREG;
  }
  
  IFS0bits.U1RXIF = 0;
}

uint16_t uart_read(char *buffer, uint16_t length) {
  uint16_t result = 0;
  
  while(uart_rx_read != uart_rx_write && length--) {
    *buffer++ = uart_rx_buffer[uart_rx_read++];
    result++;
  }
  
  U1STAbits.OERR = 0; // receiver hangs if overflow is set
  
  return result;
}

static char uart_tx_buffer[256];
static volatile uint8_t uart_tx_write;
static volatile uint8_t uart_tx_read;

void __attribute__((interrupt, no_auto_psv)) _U1TXInterrupt() {
  while(uart_tx_read != uart_tx_write && !U1STAbits.UTXBF) {
    U1TXREG = uart_tx_buffer[uart_tx_read++];
  }
  
  if(uart_tx_read == uart_tx_write) {
    IEC0bits.U1TXIE = 0;
  }
  
  IFS0bits.U1RXIF = 0;
}

void uart_write_blocking(const char *buffer, uint16_t length) {
  while(length--) {
    while(U1STAbits.UTXBF);
    
    U1TXREG = *buffer++;
  }
}

void uart_write(const char *buffer, uint16_t length) {
  while(length--) {
    uart_tx_buffer[uart_tx_write++] = *buffer++;
  }
  
  if(uart_tx_read != uart_tx_write && !U1STAbits.UTXBF) {
    U1TXREG = uart_tx_buffer[uart_tx_read++];
  }
  
  IEC0bits.U1TXIE = 1;
}

#define TO_HEX(X) ((X) > 9 ? (X) + ('A' - 10) : (X) + '0')

static void uart_write_hex(const void *data, uint16_t length) {
  for(uint16_t i = 0; i < length; i++) {
    uart_tx_buffer[uart_tx_write++] = TO_HEX(((uint8_t *)data)[i] >> 4);
    uart_tx_buffer[uart_tx_write++] = TO_HEX(((uint8_t *)data)[i] & 0xF);
  }
}

void uart_write_cmd2(const char *cmd, uint16_t length, void *p1, uint16_t p1_len, void *p2, uint16_t p2_len) {
  while(length--) {
    uart_tx_buffer[uart_tx_write++] = *cmd++;
  }
  
  uart_tx_buffer[uart_tx_write++] = ',';
  
  uart_write_hex(p1, p1_len);
  
  uart_tx_buffer[uart_tx_write++] = ',';
  
  uart_write_hex(p2, p2_len);
  
  uart_tx_buffer[uart_tx_write++] = '\r';
  
  if(uart_tx_read != uart_tx_write && !U1STAbits.UTXBF) {
    U1TXREG = uart_tx_buffer[uart_tx_read++];
  }
  
  IEC0bits.U1TXIE = 1;
}

void uart_setup() {
  RPINR18bits.U1RXR = 8; // U1RX = RB8
  RPOR3bits.RP7R = 3; // RP7/RB7 = U1TX
  
  U1MODEbits.UARTEN = 1;
  U1MODEbits.BRGH = 1;
  
  U1STAbits.UTXEN = 1;
  
  U1BRG = 34;
}
