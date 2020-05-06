#include "xc.h"

#include "bt.h"
#include "led.h"
#include "global.h"
#include "util.h"
#include "motor.h"
#include "charge.h"

static const uint8_t charge_curve[100] = {
   37,  49,  61,  73,  85,  98, 110, 118,
  126, 132, 138, 142, 146, 148, 150, 150,
  152, 154, 156, 156, 158, 160, 162, 164,
  166, 168, 171, 174, 176, 178, 179, 180,
  182, 184, 186, 186, 188, 190, 190, 192,
  192, 194, 194, 194, 196, 196, 197, 198,
  198, 198, 199, 200, 200, 201, 202, 202,
  202, 202, 203, 204, 204, 204, 205, 206,
  206, 206, 206, 207, 208, 208, 208, 209,
  210, 210, 210, 211, 212, 212, 212, 214,
  214, 214, 214, 216, 216, 217, 218, 218,
  219, 220, 220, 222, 222, 223, 224, 224,
  226, 226, 227, 228,
};

static uint8_t get_battery_level(uint16_t adc_value) {
  uint16_t upper = 99;
  uint16_t lower = 0;
  
  for(uint16_t iter = 0; iter < 10; iter++) {
    uint16_t mid = (lower + upper) / 2;
    
    if(adc_value - 512 > charge_curve[mid]) {
      lower = mid;
    } else {
      upper = mid;
    }
  }
  
  return upper;
}

void __attribute__((interrupt, no_auto_psv)) _T2Interrupt() {
  static uint16_t slowtick_count;
  static uint16_t poweroff_timer = 300; // must release button to reset counter
  
  if(PORTBbits.RB14 && !charge_detect()) { // power button
    poweroff_timer++;
    
    if(poweroff_timer == 300) {
      pwr_off();
    }
  } else {
    poweroff_timer = 0;
  }
  
  static int16_t prev_encoder_count;
  
  int16_t current_encoder_count = global.encoder_count;
  int16_t next_encoder_count = 2 * current_encoder_count - prev_encoder_count;
  
  prev_encoder_count = current_encoder_count;
  
  if(global.homing_complete) {
    // check if holder is about to slam into one of the end stops
    if((global.motor_state != MOTOR_STATE_CCW && next_encoder_count > 605) || (global.motor_state != MOTOR_STATE_CW && next_encoder_count < -5)) {
      // emergency brake
      LATBbits.LATB10 = 1;
      LATBbits.LATB11 = 1;

      TRISCbits.TRISC5 = 1;
      TRISCbits.TRISC3 = 1;
      
      global.motor_state = MOTOR_STATE_ESTOP;
      global.ign_ctl_errors++;
    } else if(global.motor_state == MOTOR_STATE_ESTOP && next_encoder_count < 550 && next_encoder_count > 50) {
      TRISCbits.TRISC5 = 0;
      TRISCbits.TRISC3 = 0;
      
      global.motor_state = MOTOR_STATE_COAST;
    }
  }
  
  slowtick_count++;
  
  // start ADC sampling
  AD1CON1bits.ADON = 1;
  
  if((slowtick_count & 0x7) == 0) {
    static uint32_t battery_avg = 712 * 32;
    
    uint32_t bat = global.raw_battery;

    battery_avg *= 63;
    battery_avg += bat * 64 + 32;
    battery_avg /= 64;

    global.battery_level = get_battery_level(battery_avg / 32);
  }
  
  IFS0bits.T2IF = 0;
}

void __attribute__((interrupt, no_auto_psv)) _T3Interrupt() {
  global.tick_count++;
  
  IFS0bits.T3IF = 0;
}

void timer_setup() {
  PR2 = 2500; // T2 == 100Hz timer
  
  T2CONbits.TCKPS = 0b10; // 1:64 prescaler
  T2CONbits.TON = 1;
  
  PR3 = 250; // T3 == 1000Hz timer
  
  T3CONbits.TCKPS = 0b10; // 1:64 prescaler
  T3CONbits.TON = 1;
}
