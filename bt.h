#ifndef _BT_H_
#define _BT_H_

uint8_t bt_is_connected();
void bt_shutdown();
void bt_send_packet(uint8_t type, void *data, uint16_t len);
void bt_recv(char recv_char);
void bt_idle();
void bt_setup();

#endif
