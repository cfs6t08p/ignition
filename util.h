#ifndef _UTIL_H_
#define _UTIL_H_

#define DISABLE_INT() __asm__ volatile ("bset SR,#7") // disable priority 4 interrupts
#define ENABLE_INT() __asm__ volatile ("bclr SR,#7") // encoder interrupts are always enabled

void pwr_off();
void idle();

#endif
