
#include "xc.h"

#include "global.h"
#include "led.h"
#include "motor.h"
#include "util.h"
#include "flash.h"
#include "bt.h"
#include "ign_api.h"

static ign_handler_t ign_handler;

void ign_call_handler(uint16_t type, void *data, uint16_t len) {
  if(ign_handler) {
    ign_handler(type, data, len);
  }
}

static uint32_t ign_get_tick_count() {
  return global.tick_count;
}

static int16_t ign_get_encoder_count() {
  return global.encoder_count;
}

static void ign_motor_stop() {
  if(global.motor_state == MOTOR_STATE_CW || global.motor_state == MOTOR_STATE_CCW) {
    motor_stop();
  } else {
    global.ign_api_errors++;
  }
}

static void ign_motor_brake() {
  if(global.motor_state == MOTOR_STATE_CW || global.motor_state == MOTOR_STATE_CCW) {
    motor_brake();
  } else {
    global.ign_api_errors++;
  }
}

static void ign_motor_set_speed(uint16_t speed) {
  if(speed < 1024) {
    motor_set_speed(speed);
  } else {
    global.ign_api_errors++;
  }
}

static void ign_motor_cw() {
  motor_cw();
}

static void ign_motor_ccw() {
  motor_ccw();
}

static void ign_pwr_off() {
  pwr_off();
}

static void ign_set_led_constant(uint16_t led, uint8_t intensity) {
  if(led < 4) {
    led_set_constant(led, intensity);
  } else {
    global.ign_api_errors++;
  }
}

static void ign_set_led_pulsing(uint16_t led, uint8_t rate) {
  if(led < 4) {
    led_set_pulsing(led, rate);
  } else {
    global.ign_api_errors++;
  }
}

static uint8_t ign_is_connected() {
  return bt_is_connected();
}

static void ign_send_packet(void *data, uint16_t len) {
  if(len <= 19) {
    bt_send_packet(0, data, len);
  } else {
    global.ign_api_errors++;
  }
}

static uint8_t ign_get_battery_level() {
  return 50; // TODO
}

static uint16_t ign_get_api_errors() {
  return global.ign_api_errors;
}

static uint16_t ign_get_ctl_errors() {
  return global.ign_ctl_errors;
}

static uint16_t ign_get_motor_current() {
  return global.motor_current;
}

static uint16_t ign_get_raw_battery_level() {
  return global.battery_level;
}

static uint16_t ign_get_touch_data(uint8_t channel) {
  if(channel < 7) {
    return global.touch[channel];
  } else {
    global.ign_api_errors++;
    
    return 0;
  }
}

static void ign_idle() {
  idle();
}

static void ign_set_handler(ign_handler_t handler) {
  ign_handler = handler;
}

const struct __attribute__ ((space(psv))) ign_call_table _IGN_CALL_TABLE = {
  ign_get_tick_count,
  ign_get_encoder_count,
  ign_motor_stop,
  ign_motor_brake,
  ign_motor_set_speed,
  ign_motor_cw,
  ign_motor_ccw,
  ign_pwr_off,
  ign_set_led_constant,
  ign_set_led_pulsing,
  ign_is_connected,
  ign_send_packet,
  ign_get_battery_level,
  ign_get_api_errors,
  ign_get_ctl_errors,
  ign_get_motor_current,
  ign_get_raw_battery_level,
  ign_get_touch_data,
  ign_idle,
  ign_set_handler,
};

extern void _IGN_MAIN();
extern uint16_t _IGN_SIGNATURE;

void ign_run() {
  IGN = &_IGN_CALL_TABLE;
  
  if(read_flash_word(&_IGN_SIGNATURE) == 0x1671) {
    _IGN_MAIN();
  }
}
