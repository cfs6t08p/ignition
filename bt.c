#include <string.h>

#include "xc.h"

#include "uart.h"
#include "led.h"
#include "motor.h"
#include "util.h"
#include "global.h"
#include "boot.h"
#include "ign.h"
#include "ign_api.h"
#include "flash.h"

#define FROM_HEX(X) ((X) >= 'A' ? ((X) - 'A') + 10 : (X) - '0')

static uint8_t bt_connected;

#define BT_MODE_DATA 0  // RN4871 in data mode
#define BT_MODE_CMD 1   // command in progress
#define BT_MODE_READY 2 // ready to accept new command

static uint8_t bt_mode;

#define BT_STATE_IDLE 0
#define BT_STATE_STATUS 1
#define BT_STATE_PARAM1 2
#define BT_STATE_PARAM2 3
#define BT_STATE_LS 4

static uint16_t bt_state; // output parser state

#define BT_INIT_SEQ_WAIT 0    // waiting for module to come out of reset
#define BT_INIT_SEQ_LS 1      // list services and characteristics
#define BT_INIT_SEQ_SN 2      // set name to "Launch"
#define BT_INIT_SEQ_PZ 3      // clear service table
#define BT_INIT_SEQ_PS 4      // create service
#define BT_INIT_SEQ_PC1 5     // create characteristics
#define BT_INIT_SEQ_PC2 6
#define BT_INIT_SEQ_PC3 7
#define BT_INIT_SEQ_PCC 8
#define BT_INIT_SEQ_PCD 9
#define BT_INIT_SEQ_REBOOT 10 // send reboot command
#define BT_INIT_SEQ_DONE 11

static uint8_t bt_init_seq; // initialization sequence

#define BT_NUM_HANDLES 5

#define BT_HANDLE_DATA 0
#define BT_HANDLE_SENSOR 1
#define BT_HANDLE_CTL 2
#define BT_HANDLE_IGNI 3
#define BT_HANDLE_IGNO 4

static uint16_t bt_handles_found;
static uint16_t bt_handles[BT_NUM_HANDLES];

static const char bt_handle_id[BT_NUM_HANDLES] = { '1', '2', '3', 'C', 'D' };

uint8_t bt_is_connected() {
  return bt_connected;
}

void bt_shutdown() {
  if(bt_connected) {
    UART_WRITE_BLOCKING("K,1\r"); // force disconnect
  }
}

void bt_set_ctl(uint8_t status) {
  if(bt_connected) {
    UART_WRITE_CMD2("SHW", &bt_handles[BT_HANDLE_CTL], 2, &status, 1);
  }
}

void bt_set_data(void *data, uint16_t len) {
  if(bt_connected) {
    UART_WRITE_CMD2("SHW", &bt_handles[BT_HANDLE_DATA], 2, data, len);
  }
}

void bt_set_sensor(uint16_t data) {
  if(bt_connected) {
    UART_WRITE_CMD2("SHW", &bt_handles[BT_HANDLE_SENSOR], 2, &data, 2);
  }
}

void bt_send_packet(uint8_t type, void *data, uint16_t len) {
  static char buffer[20];
  
  if(len > 19) {
    len = 19;
  }
  
  buffer[0] = type;
  memcpy(&buffer[1], data, len);
  
  UART_WRITE_CMD2("SHW", &bt_handles[BT_HANDLE_IGNO], 2, buffer, len + 1);
}

static void bt_ls_characteristic(char *uuid, uint16_t length, char *handle, uint16_t h_len) {
  if(length == 34 && h_len == 2) {
    for(int i = 0; i < BT_NUM_HANDLES; i++) {
      if(uuid[9] == bt_handle_id[i]) { // two spaces + 88F8058 == 9
        bt_handles[i] = *((uint16_t *)handle);
        
        bt_handles_found++;
      }
    }
  }
}

static void bt_next_cmd() {
  if(bt_init_seq != BT_INIT_SEQ_DONE) {
    bt_mode = BT_MODE_CMD;
    
    if(bt_init_seq == BT_INIT_SEQ_LS) {
      UART_WRITE_STR("LS\r");
      bt_state = BT_STATE_STATUS;
      bt_handles_found = 0;
    } else if(bt_init_seq == BT_INIT_SEQ_SN) {
      UART_WRITE_STR("SN,Launch\r");
      bt_init_seq = BT_INIT_SEQ_PZ;
    } else if(bt_init_seq == BT_INIT_SEQ_PZ) {
      UART_WRITE_STR("PZ\r");
      bt_init_seq = BT_INIT_SEQ_PS;
    } else if(bt_init_seq == BT_INIT_SEQ_PS) {
      UART_WRITE_STR("PS,88F80580000001E6AACE0002A5D5C51B\r");
      bt_init_seq = BT_INIT_SEQ_PC1;
    } else if(bt_init_seq == BT_INIT_SEQ_PC1) {
      UART_WRITE_STR("PC,88F80581000001E6AACE0002A5D5C51B,0E,14\r");
      bt_init_seq = BT_INIT_SEQ_PC2;
    } else if(bt_init_seq == BT_INIT_SEQ_PC2) {
      UART_WRITE_STR("PC,88F80582000001E6AACE0002A5D5C51B,12,14\r");
      bt_init_seq = BT_INIT_SEQ_PC3;
    } else if(bt_init_seq == BT_INIT_SEQ_PC3) {
      UART_WRITE_STR("PC,88F80583000001E6AACE0002A5D5C51B,0A,08\r");
      bt_init_seq = BT_INIT_SEQ_PCC;
    } else if(bt_init_seq == BT_INIT_SEQ_PCC) {
      UART_WRITE_STR("PC,88F8058C000001E6AACE0002A5D5C51B,04,14\r");
      bt_init_seq = BT_INIT_SEQ_PCD;
    } else if(bt_init_seq == BT_INIT_SEQ_PCD) {
      UART_WRITE_STR("PC,88F8058D000001E6AACE0002A5D5C51B,10,14\r");
      bt_init_seq = BT_INIT_SEQ_REBOOT;
    } else if(bt_init_seq == BT_INIT_SEQ_REBOOT) {
      UART_WRITE_STR("R,1\r");
      bt_init_seq = BT_INIT_SEQ_WAIT;
    }
  }
}

static void bt_status0(char *status, uint16_t length) {
  if(!strncmp(status, "DISCONNECT", length)) {
    bt_connected = 0;
    
    led_set_pulsing(BLED_B, 2);
  } else if(!strncmp(status, "REBOOT", length)) {
    led_set_pulsing(BLED_B, 0);
    
    UART_WRITE_STR("$$$");
    
    bt_init_seq = BT_INIT_SEQ_LS;
  }
}

static void bt_status1(char *status, uint16_t length, char *p1, uint16_t p1_len) {
  
}

static void bt_status2(char *status, uint16_t length, char *p1, uint16_t p1_len, char *p2, uint16_t p2_len) {
  if(!strncmp(status, "CONNECT", length)) {
    bt_connected = 1;
    
    led_set_constant(BLED_B, 255);
  } else if(!strncmp(status, "WV", length) && p1_len == 2) {
    uint16_t handle = *((uint16_t *)p1);
    
    static uint8_t last_ctl;
    
    if(handle == bt_handles[BT_HANDLE_DATA]) {
      if(p2_len == 2) {
        if(last_ctl == 0xD && p2[0] == 'O' && p2[1] == 'K') {
          modelock();
        } else {
          ign_call_handler(IGN_EVENT_FL13, p2, p2_len);
        }
      } else if(p2_len == 1) {
        if(last_ctl == 3) { // get execution mode
          uint8_t mode[2] = { 0x00, 0x01 };
          
          mode[0] = 50; // TODO: battery level
          
          bt_set_data(mode, 2);
          bt_set_ctl(2);
          
        } else if(last_ctl == 5) { // get version
          uint8_t version[12] = { 0x00, 0x00, 0x00, 0x9B, 0x01, 0x01, 0x00, 0x00, 0x00, 0xEB, 0x01, 0x03 };
          
          // TODO: fetch data from flash
          
          bt_set_data(version, 12);
          bt_set_ctl(2);
        } else if(last_ctl == 6) { // set execution mode
          bt_shutdown();
          enter_bootloader();
        } else if(last_ctl == 7) { // get CRC
          uint16_t crc = read_crc();
          
          bt_set_data(&crc, 2);
          bt_set_ctl(2);
        }
      }
      
      last_ctl = 0;
    } else if(handle == bt_handles[BT_HANDLE_CTL]) {
      if(p2_len == 1) {
        last_ctl = *((uint8_t *)p2);
      }
    } else if(handle == bt_handles[BT_HANDLE_IGNI]) {
      uint8_t type = *p2;
      
      if(type == 0) { // app data
        ign_call_handler(IGN_EVENT_DATA, &p2[1], p2_len - 1);
      } else {
        flash_handle_packet(type, &p2[1], p2_len - 1);
      }
    }
  }
}

void bt_recv(char recv_char) {
  static char bt_status_buffer[40];
  static uint16_t bt_status_length;

  static char bt_param1_buffer[16];
  static uint16_t bt_param1_nibbles;

  static char bt_param2_buffer[20];
  static uint16_t bt_param2_nibbles;
  
  static uint32_t bt_timeout;
  
  if(recv_char == '%') {
    if(bt_state == BT_STATE_IDLE || global.tick_count > bt_timeout) {
      bt_state = BT_STATE_STATUS;
      bt_status_length = 0;
      bt_timeout = global.tick_count + 100;
    } else if(bt_state == BT_STATE_STATUS) {
      bt_status0(bt_status_buffer, bt_status_length);
      bt_state = BT_STATE_IDLE;
    } else if(bt_state == BT_STATE_PARAM1) {
      bt_status1(bt_status_buffer, bt_status_length, bt_param1_buffer, (bt_param1_nibbles + 1) / 2);
      bt_state = BT_STATE_IDLE;
    } else if(bt_state == BT_STATE_PARAM2) {
      bt_status2(bt_status_buffer, bt_status_length, bt_param1_buffer, (bt_param1_nibbles + 1) / 2, bt_param2_buffer, (bt_param2_nibbles + 1) / 2);
      bt_state = BT_STATE_IDLE;
    }
  } else if(bt_init_seq == BT_INIT_SEQ_LS && recv_char == '\n') {
    if(bt_state == BT_STATE_IDLE || bt_state == BT_STATE_STATUS) {
      bt_state = BT_STATE_STATUS;
      bt_status_length = 0;
    } else if(bt_state == BT_STATE_PARAM2) {
      if(bt_param2_nibbles == 3) { // skip configuration handles
        bt_ls_characteristic(bt_status_buffer, bt_status_length, bt_param1_buffer, (bt_param1_nibbles + 1) / 2);
      }
      
      bt_state = BT_STATE_STATUS;
      bt_status_length = 0;
    }
  } else {
    if(bt_state == BT_STATE_IDLE) {
      if(bt_mode != BT_MODE_READY && recv_char == '>') { // received "CMD>"
        bt_mode = BT_MODE_READY;

        if(bt_init_seq == BT_INIT_SEQ_LS) {
          bt_state = BT_STATE_STATUS;
          bt_status_length = 0;
        }

        bt_next_cmd();
      }
    } else if(bt_state == BT_STATE_STATUS) {
      if(bt_init_seq == BT_INIT_SEQ_LS && recv_char == 'N') { // look for "END"
        if(bt_handles_found != BT_NUM_HANDLES) {
          bt_init_seq = BT_INIT_SEQ_SN;
          bt_state = BT_STATE_IDLE;
        } else {
          bt_init_seq = BT_INIT_SEQ_DONE;
          led_set_pulsing(BLED_B, 2);
        }
      } else if(recv_char == ',') {
        bt_state = BT_STATE_PARAM1;
        bt_param1_nibbles = 0;
      } else if(bt_status_length < sizeof(bt_status_buffer)) {
        bt_status_buffer[bt_status_length++] = recv_char;
      }
    } else if(bt_state == BT_STATE_PARAM1) {
      if(recv_char == ',') {
        bt_state = BT_STATE_PARAM2;
        bt_param2_nibbles = 0;
      } else if(bt_param1_nibbles < sizeof(bt_param1_buffer) * 2) {
        if(bt_param1_nibbles & 1) {
          bt_param1_buffer[bt_param1_nibbles / 2] |= FROM_HEX(recv_char);
        } else {
          bt_param1_buffer[bt_param1_nibbles / 2] = FROM_HEX(recv_char) << 4;
        }

        bt_param1_nibbles++;
      }
    } else if(bt_state == BT_STATE_PARAM2) {
      if(bt_param2_nibbles < sizeof(bt_param2_buffer) * 2) {
        if(bt_param2_nibbles & 1) {
          bt_param2_buffer[bt_param2_nibbles / 2] |= FROM_HEX(recv_char);
        } else {
          bt_param2_buffer[bt_param2_nibbles / 2] = FROM_HEX(recv_char) << 4;
        }

        bt_param2_nibbles++;
      }
    }
  }
}

static void bt_release_reset() {
  DISABLE_INT();
  LATCbits.LATC6 = 1; // release BT module reset
  ENABLE_INT();
}

static uint16_t bt_reset;

void bt_idle() {
  if(global.tick_count > 50 && !bt_reset) {
    bt_release_reset();
    
    bt_reset = 1;
  }
  
  char recv_char;
  
  if(uart_read(&recv_char, 1)) {
    bt_recv(recv_char);
  }
}

void bt_setup() {
  LATCbits.LATC6 = 0;
  LATCbits.LATC9 = 1;
  LATCbits.LATC7 = 0;
  
  TRISCbits.TRISC6 = 0;
  TRISCbits.TRISC9 = 0;
  TRISCbits.TRISC7 = 0;
}
