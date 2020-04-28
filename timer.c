#include "xc.h"

#include "bt.h"
#include "led.h"
#include "global.h"
#include "util.h"
#include "motor.h"
#include "charge.h"

void __attribute__((interrupt, no_auto_psv)) _T2Interrupt() {
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
      
      global.motor_state = MOTOR_STATE_BRAKE;
      global.ign_ctl_errors++;
    } else if(global.motor_state == MOTOR_STATE_BRAKE && next_encoder_count < 550 && next_encoder_count > 50) {
      TRISCbits.TRISC5 = 0;
      TRISCbits.TRISC3 = 0;
      
      global.motor_state = MOTOR_STATE_COAST;
    }
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
