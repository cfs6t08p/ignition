#include "xc.h"

#include "bt.h"
#include "charge.h"
#include "motor.h"
#include "uart.h"
#include "led.h"
#include "timer.h"
#include "boot.h"
#include "global.h"
#include "ign.h"
#include "flash.h"
#include "adc.h"
#include "util.h"

volatile struct global_data global;

static void pwr_on() {
  LATAbits.LATA7 = 1;
  TRISAbits.TRISA7 = 0;
}

void pwr_off() {
  IEC0 = 0;
  IEC1 = 0;
  
  motor_stop();
  
  bt_shutdown();
  
  LATCbits.LATC4 = 0;
  LATBbits.LATB13 = 0;
  
  TRISCbits.TRISC4 = 0;
  TRISBbits.TRISB13 = 0;
  
  LATAbits.LATA7 = 0;
  
  LATAbits.LATA8 = 1;
  LATAbits.LATA4 = 1;
  LATAbits.LATA9 = 1;
  LATBbits.LATB5 = 1;
  
  while(1) __builtin_clrwdt();
}

void idle() {
  bt_idle();
  led_idle();
  flash_idle();
  
  static uint16_t last_battery_level;
  
  uint16_t battery_level = global.battery_level;
  
  if(battery_level < last_battery_level) {
    if(battery_level < 15) {
      led_set_pulsing(PLED_R, 2);
      led_set_constant(PLED_G, 0);
    } else if(battery_level < 30) {
      led_set_constant(PLED_R, 250);
      led_set_constant(PLED_G, 0);
    }
    
    last_battery_level = battery_level;
  }
  
  if(charge_detect()) {
    motor_stop();
    
    __asm__ volatile ("reset");
  }
  
  __builtin_clrwdt();
}

static void enable_interrupts() {
  IEC1bits.INT1IE = 1;
  IEC1bits.INT2IE = 1;
  IEC0bits.U1RXIE = 1;
  IEC0bits.T2IE = 1;
  IEC0bits.T3IE = 1;
  IEC0bits.AD1IE = 1;
}

int main(void) {
  CORCONbits.PSV = 1;
  PSVPAG = 0;
  INTCON1bits.NSTDIS = 0; // interrupt nesting enabled
  CLKDIVbits.CPDIV = 0; // 32MHz
  INTCON2bits.ALTIVT = 0;
  
  if(RCONbits.BOR && !RCONbits.POR) {
    // brown out reset triggers when the power latch is released and the 3V3 rail drops, just wait for power off
    while(1) __builtin_clrwdt();
  }
  
  pwr_on();
  
  uint8_t software_reset = RCONbits.SWR; // reset from bootloader
    
  RCON = 0;
  
  AD1PCFG = 0xFFFF; // set all analog pins to digital

  charge_detect_setup();
  led_setup();
  motor_setup();
  uart_setup();
  bt_setup();
  timer_setup();
  adc_setup();
  
  if(charge_detect()) {
    enable_interrupts();
    
    charge_loop();
    
    pwr_off();
  }
  
  if(check_bootflag() && !software_reset) {
    volatile uint32_t on_delay = 1000000;

    // must hold power button for ~1 second
    while(PORTBbits.RB14 && on_delay--) __builtin_clrwdt();

    if(!PORTBbits.RB14) {
      // spurious powerup or button was released early, wait a few seconds then turn off
      // back EMF from the motor can trigger the power mosfet if the holder is moved manually
      // turning off immediately can lead to repeated power cycling which may destroy the bluetooth module
      DELAY(5000000);

      pwr_off();
    }
  }
  
  led_set_constant(BLED_R, 0);
  led_set_constant(BLED_B, 0);
  led_set_constant(PLED_G, 250);
  led_set_constant(PLED_R, 0);
  
  enable_interrupts();
  
  motor_home();
  
  uint32_t homing_count = 0;
  
  while(motor_homing_loop()) {
    homing_count++;
    idle();
    
    // if homing does not finish within a reasonable number of cycles, abort and flash red LED
    if(homing_count > 100000) {
      motor_stop();
      led_set_constant(BLED_B, 0);
      led_set_pulsing(BLED_R, 0);
      
      while(1) {
        idle();
      }
    }
  }
  
  ign_run();
  
  // no app installed
  led_set_constant(PLED_R, 0);
  led_set_pulsing(PLED_G, 2);

  while(1) {
    idle();
  }

  return 0;
}
