#include "xc.h"

#include "motor.h"
#include "boot.h"
#include "flash.h"

extern uint16_t _MBR_CRC;
extern uint16_t _MBR_BOOTFLAG;

uint16_t read_crc() {
  return read_flash_word(&_MBR_CRC);
}

void enter_bootloader() {
  write_flash_word(&_MBR_BOOTFLAG, 0);
  __asm__ volatile ("reset");
}

int check_bootflag() {
  return read_flash_word(&_MBR_BOOTFLAG) == 0xA5A5;
}

void modelock() {
  write_flash_word(&_MBR_BOOTFLAG, 0xA5A5);
}
