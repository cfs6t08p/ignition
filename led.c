#include "xc.h"

#include "led.h"
#include "global.h"
#include "util.h"

void led_setup() {
  LATAbits.LATA8 = 1;
  LATAbits.LATA4 = 1;
  LATAbits.LATA9 = 0;
  LATBbits.LATB5 = 0;
  
  TRISAbits.TRISA8 = 0; // BLED_R
  TRISAbits.TRISA4 = 0; // BLED_B
  TRISAbits.TRISA9 = 0; // PLED_G
  TRISBbits.TRISB5 = 0; // PLED_R
}

static uint8_t led_intensity[4];
static uint8_t led_pulse_rate[4];

void led_set_constant(uint16_t led, uint8_t intensity) {
  led_pulse_rate[led] = 0xFF;
  led_intensity[led] = intensity;
}

void led_set_pulsing(uint16_t led, uint8_t rate) {
  led_pulse_rate[led] = rate;
}

void led_idle() {
  static uint32_t last_led_tick;
  static uint8_t led_state[4];
  
  uint32_t tick = global.tick_count;
  
   // soft PWM LEDs
  if(tick != last_led_tick) {
    for(uint16_t i = 0; i < 4; i++) {
      int16_t r = TMR3; // 0-249
      
      if(led_pulse_rate[i] != 0xFF) {
        int16_t t = ((tick >> led_pulse_rate[i]) & 0x1FF); // 0-511
        
        if(t < 0x100) {
          led_state[i] = r + 5 > t; // 0-5 == off, 6-255 == 0-100%
        } else {
          led_state[i] = r < t - 0x100; // 256-505 == 100-0%, 506-511 == off
        }
      } else {
        led_state[i] = r >= led_intensity[i];
      }
    }
  }
  
  DISABLE_INT();
  
  LATAbits.LATA8 = led_state[0];
  LATAbits.LATA4 = led_state[1];
  LATAbits.LATA9 = led_state[2];
  LATBbits.LATB5 = led_state[3];
  
  ENABLE_INT();
}
