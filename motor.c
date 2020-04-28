#include "xc.h"

#include "global.h"
#include "util.h"

void motor_set_speed(uint16_t speed) {
  OC1R = speed;
}

void motor_stop() {
  DISABLE_INT();
  
  LATBbits.LATB10 = 1;
  LATBbits.LATB11 = 1;
  
  TRISCbits.TRISC5 = 0;
  TRISCbits.TRISC3 = 0;
  
  motor_set_speed(0);
  
  global.motor_state = MOTOR_STATE_COAST;
  
  ENABLE_INT();
}

void motor_cw() {
  DISABLE_INT();
  
  LATBbits.LATB10 = 1;
  LATBbits.LATB11 = 0;
  
  TRISCbits.TRISC5 = 1;
  TRISCbits.TRISC3 = 1;
  
  global.motor_state = MOTOR_STATE_CW;
  
  ENABLE_INT();
}

void motor_ccw() {
  DISABLE_INT();
  
  LATBbits.LATB10 = 0;
  LATBbits.LATB11 = 1;
  
  TRISCbits.TRISC5 = 1;
  TRISCbits.TRISC3 = 1;
  
  global.motor_state = MOTOR_STATE_CCW;
  
  ENABLE_INT();
}

void motor_brake() {
  DISABLE_INT();
  
  LATBbits.LATB10 = 1;
  LATBbits.LATB11 = 1;
  
  TRISCbits.TRISC5 = 1;
  TRISCbits.TRISC3 = 1;
  
  global.motor_state = MOTOR_STATE_BRAKE;
  
  ENABLE_INT();
}

void motor_setup() {
  motor_stop();
  motor_set_speed(0);

  RPOR2bits.RP5R = 18; // RP5/RA0 = OC1
  TRISBbits.TRISB10 = 0;
  TRISBbits.TRISB11 = 0;
  
  TRISCbits.TRISC5 = 0;
  TRISCbits.TRISC3 = 0;
  
  LATCbits.LATC5 = 0;
  LATCbits.LATC3 = 0;

  OC1CON2bits.SYNCSEL = 0b11111; // This OC module
  OC1CON2bits.OCTRIG = 0; // Synchronize

  OC1CON1bits.OCTSEL = 0b111;
  OC1CON1bits.OCM = 0b110;

  OC1RS = 1023;
  
  TRISBbits.TRISB4 = 1;
  
  TRISCbits.TRISC4 = 1; // ENCA
  TRISBbits.TRISB13 = 1; // ENCB
  
  RPINR0bits.INT1R = 20; // INT1 = RC4
  RPINR1bits.INT2R = 13; // INT2 = RB13
  
  IPC5bits.INT1IP = 7; // set encoder interrupts to priority 7
  IPC7bits.INT2IP = 7; // encoder interrupts can nest into other interrupts
}

static uint8_t homing_state;
static int32_t homing_avg;

static void check_hall_sensor() {
  if(PORTBbits.RB4) { // outside bottom area
    if(homing_state == 2 || homing_state == 4) {
      homing_avg += global.encoder_count;
      motor_ccw();
      homing_state--;
    }
  } else { // in bottom area
    if(homing_state == 5) {
      motor_cw();
      homing_state--;
    }
    
    if(homing_state == 3) {
      homing_avg += global.encoder_count;
      motor_cw();
      homing_state--;
    }
    
    if(homing_state == 1) {
      homing_avg += global.encoder_count;
      motor_stop();
      homing_state--;
      
      global.encoder_count -= homing_avg / 4 - 44;
      global.homing_complete = 1;
    }
  }
}

void motor_home() {
  if(PORTBbits.RB4) { // outside bottom area
    motor_ccw();
    
    homing_state = 5;
  } else { // in bottom area
    motor_cw();
    
    homing_state = 4;
  }
  
  motor_set_speed(192);
}

uint16_t motor_homing_loop() {
  check_hall_sensor();
  
  return homing_state > 0;
}

void __attribute__((interrupt, no_auto_psv)) _INT1Interrupt() {
  if(INTCON2bits.INT1EP) {
    if(PORTBbits.RB13) {
      global.encoder_count++;
    } else {
      global.encoder_count--;
    }
    
    INTCON2bits.INT1EP = 0;
  } else {
    if(PORTBbits.RB13) {
      global.encoder_count--;
    } else {
      global.encoder_count++;
    }
    
    INTCON2bits.INT1EP = 1;
  }
  
  IFS1bits.INT1IF = 0;
}

void __attribute__((interrupt, no_auto_psv)) _INT2Interrupt() {
  if(INTCON2bits.INT2EP) {
    if(PORTCbits.RC4) {
      global.encoder_count--;
    } else {
      global.encoder_count++;
    }
    
    INTCON2bits.INT2EP = 0;
  } else {
    if(PORTCbits.RC4) {
      global.encoder_count++;
    } else {
      global.encoder_count--;
    }
    
    INTCON2bits.INT2EP = 1;
  }
  
  IFS1bits.INT2IF = 0;
}
