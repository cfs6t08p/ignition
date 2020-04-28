#ifndef _BOOT_H_
#define _BOOT_H_

uint16_t read_crc();
void enter_bootloader();
int check_bootflag();
void modelock();

#endif
