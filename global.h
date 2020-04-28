#ifndef _GLOBAL_H_
#define _GLOBAL_H_

#define MOTOR_STATE_COAST 0
#define MOTOR_STATE_CW 1
#define MOTOR_STATE_CCW 2
#define MOTOR_STATE_BRAKE 3

struct global_data {
  uint32_t tick_count;
  
  uint8_t motor_state;
  uint8_t homing_complete;
  int16_t encoder_count;
  
  uint16_t ign_api_errors;
  uint16_t ign_ctl_errors;
  
  uint16_t motor_current;
  uint16_t touch[7];
  uint16_t battery_level;
};

extern volatile struct global_data global;

#endif
