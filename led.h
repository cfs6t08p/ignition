#ifndef _LED_H_
#define _LED_H_

#define BLED_R 0
#define BLED_B 1
#define PLED_G 2
#define PLED_R 3

void led_setup();
void led_set_constant(uint16_t led, uint8_t intensity);
void led_set_pulsing(uint16_t led, uint8_t rate);
void led_idle();

#endif
