#ifndef _UTIL_H_
#define _UTIL_H_

#define DISABLE_INT() __asm__ volatile ("bset SR,#7") // disable priority 4 interrupts
#define ENABLE_INT() __asm__ volatile ("bclr SR,#7") // encoder interrupts are always enabled

#define DELAY(X) { volatile uint32_t _delay = (X); while(_delay--) __builtin_clrwdt(); }

void pwr_off();
void idle();

#endif
