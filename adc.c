#include "xc.h"

#include "global.h"

void __attribute__((interrupt, no_auto_psv)) _ADC1Interrupt() {
  global.motor_current = ADC1BUF0;
  global.touch[0] = ADC1BUF1;
  global.touch[1] = ADC1BUF2;
  global.touch[2] = ADC1BUF3;
  global.touch[3] = ADC1BUF4;
  global.touch[4] = ADC1BUF5;
  global.touch[5] = ADC1BUF6;
  global.touch[6] = ADC1BUF7;
  global.battery_level = ADC1BUF8;
  
  IFS0bits.AD1IF = 0;
}

void adc_setup() {
  AD1CON1bits.SSRC = 0b111;
  AD1CON1bits.ASAM = 1;
  
  AD1CON2bits.CSCNA = 1;
  AD1CON2bits.SMPI = 8;
  
  AD1CON3bits.SAMC = 31;
  AD1CON3bits.ADCS = 63;
  
  AD1PCFG = 0xFC01;
  
  AD1CSSL = ~0xFC01;
  
  AD1CON1bits.ADON = 1;
}
