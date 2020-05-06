
#include "xc.h"

#include "util.h"
#include "boot.h"
#include "led.h"
#include "global.h"

void charge_detect_setup() {
  TRISAbits.TRISA10 = 1; // CHG_DETECT
}

uint16_t charge_detect() {
  return PORTAbits.RA10;
}

void charge_loop() {
  uint32_t safe_mode_count = 0;
  
  led_set_constant(BLED_R, 0);
  led_set_constant(BLED_B, 0);
  
  while(charge_detect()) {
    led_set_constant(PLED_G, (global.battery_level * 25) / 10);
    led_set_constant(PLED_R, 250 - ((global.battery_level * 25) / 10));
    
    // failsafe: hold power button while charging to enter bootloader
    if(PORTBbits.RB14) {
      safe_mode_count++;
      
      // flash red LED before
      if(safe_mode_count == 150000) {
        led_set_constant(BLED_R, 250);
      }
      
      if(safe_mode_count == 200000) {
        enter_bootloader();
      }
    } else {
      safe_mode_count = 0;
      led_set_constant(BLED_R, 0);
    }
    
    led_idle();
    __builtin_clrwdt();
  }
}
