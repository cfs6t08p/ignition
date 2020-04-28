#ifndef _FLASH_H_
#define _FLASH_H_

uint32_t read_flash_word(void *addr);
void write_flash_word(void *addr, uint32_t value);

void flash_idle();
void flash_handle_packet(uint8_t type, void *data, uint16_t len);

#endif
