#include <string.h>

#include "xc.h"

#include "bt.h"
#include "util.h"
#include "global.h"
#include "led.h"
#include "boot.h"

uint32_t read_flash_word(void *addr) {
  return (uint32_t)__builtin_tblrdl((int)addr) | ((uint32_t)__builtin_tblrdh((int)addr) << 16);
}

void write_flash_word(void *addr, uint32_t value) {
  NVMCONbits.WREN = 1;
  NVMCONbits.NVMOP = 0b0011; // write word
  
  __builtin_tblwtl((int)addr, value & 0xFFFF);
  __builtin_tblwth((int)addr, value >> 16);
  __builtin_disi(5);
  __builtin_write_NVM();
}

#define PAGE_SIZE_WORDS 0x200
#define BLOCK_SIZE_WORDS 0x40

#define PAGE_SIZE_BYTES 0x300
#define BLOCK_SIZE_BYTES 0x60

static void flash_erase_page(void *addr) {
  __builtin_tblwtl((uint16_t)addr, 0x0000); // Set base address of erase block
  // with dummy latch write
  NVMCON = 0x4042; // Initialize NVMCON
  asm("DISI #5");
  __builtin_write_NVM();
}

extern uint8_t _IGN_START;
extern uint8_t _IGN_END;
extern uint16_t _IGN_SIGNATURE;

static void flash_erase_app() {
  for(uint8_t *page = &_IGN_START; page < &_IGN_END; page += PAGE_SIZE_WORDS) {
    __builtin_clrwdt();
    flash_erase_page(page);
  }
}

// type 0 is ign data
#define FLASH_PACKET_START 1
#define FLASH_PACKET_GETBLOCK 2
#define FLASH_PACKET_BLOCKDATA 3
#define FLASH_PACKET_SUCCESS 4
#define FLASH_PACKET_FAIL 5

#define SEGMENT_WORDS 6

struct __attribute__((packed)) flash_packet {
  union {
    struct {
      uint32_t magic;
      uint32_t crc;
      uint16_t blocks;
    } __attribute__((packed)) start;
    
    struct {
      uint16_t blocknum;
      uint16_t segments;
    } __attribute__((packed)) getblock;
    
    struct {
      uint8_t segment:4;
      uint8_t blocknum:4;
      uint8_t data[SEGMENT_WORDS * 3];
    } __attribute__((packed)) blockdata;
    
    struct {
      uint16_t reason;
      uint32_t crc;
    } __attribute__((packed)) fail;
  };
};

static struct flash_packet packet;

static uint8_t update_started;
static uint16_t num_blocks;
static uint16_t current_block;
static uint16_t received_segments;

static uint32_t target_crc;

static const uint32_t crc32_tbl[] = {
  0x00000000, 0x1db71064, 0x3b6e20c8, 0x26d930ac,
  0x76dc4190, 0x6b6b51f4, 0x4db26158, 0x5005713c,
  0xedb88320, 0xf00f9344, 0xd6d6a3e8, 0xcb61b38c,
  0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c
};

static uint32_t crc32(uint8_t *ptr, int cnt, uint32_t crc)
{
  crc = ~crc;
  
  while (cnt--) {
    crc = (crc >> 4) ^ crc32_tbl[(crc & 0xf) ^ (*ptr & 0xf)];
    crc = (crc >> 4) ^ crc32_tbl[(crc & 0xf) ^ (*(ptr++) >> 4)];
  }
  
  return ~crc;
}

void flash_idle() {
  if(update_started) {
    update_started = 0;
    
    led_set_constant(PLED_R, 0);
    led_set_pulsing(PLED_G, 1);
    
    flash_erase_app();
    
    NVMCON = 0x4001; // prepare row write
    
    CRCCON1bits.CRCEN = 0;
    
    current_block = 0;
    
    while(current_block < num_blocks) {
      packet.getblock.blocknum = current_block;
      packet.getblock.segments = ~received_segments;
      
      // request all missing segments from current block
      bt_send_packet(FLASH_PACKET_GETBLOCK, &packet, sizeof(packet));
      
      uint32_t timeout = global.tick_count + 5000;
      
      while(global.tick_count < timeout) {
        __builtin_clrwdt();
        bt_idle();
        led_idle();
        
        // all segments received, write row
        if(received_segments == 0b11111111111) {
          asm("DISI #5");
          __builtin_write_NVM();
          
          NVMCON = 0x4001; // prepare next row write
          
          current_block++;
          received_segments = 0;
          break;
        }
      }
    }
    
    uint32_t crc = 0;
    
    uint16_t addr = ((uint16_t)&_IGN_START);
    uint16_t end = addr + num_blocks * BLOCK_SIZE_WORDS * 2;
    uint16_t tmp[2];
    
    while(addr < end) {
      tmp[0] = __builtin_tblrdl(addr);
      tmp[1] = __builtin_tblrdh(addr);
      
      crc = crc32((void *)tmp, 4, crc);
      
      addr += 2;
      
      __builtin_clrwdt();
    }
    
    if(crc == target_crc) {
      bt_send_packet(FLASH_PACKET_SUCCESS, &packet, sizeof(packet));
      
      // wait for success packet to be sent
      DELAY(500000);
      
      bt_shutdown();
      
      // wait for disconnect
      DELAY(500000);
      
      // update complete, write signature & reset
      write_flash_word(&_IGN_SIGNATURE, 0x1671);
      
      modelock();

      __asm__ volatile ("reset");
    } else {
      packet.fail.reason = 0; // crc mismatch
      packet.fail.crc = crc;
      bt_send_packet(FLASH_PACKET_FAIL, &packet, sizeof(packet));
    }
  }
}

void flash_handle_packet(uint8_t type, void *data, uint16_t len) {
  struct flash_packet *input = (void *)data;
  
  if(len == sizeof(*input)) {
    if(type == FLASH_PACKET_START) {
      if(input->start.magic == 0x16711671 && input->start.blocks <= (&_IGN_END - &_IGN_START) / BLOCK_SIZE_BYTES) {
        num_blocks = input->start.blocks;
        target_crc = input->start.crc;
        update_started = 1;
      }
    } else if(type == FLASH_PACKET_BLOCKDATA) {
      if(input->blockdata.segment < 11 && input->blockdata.blocknum == (current_block & 0xF)) {
        received_segments |= 1 << input->blockdata.segment;
        
        uint16_t block_offset = input->blockdata.segment * SEGMENT_WORDS;
        uint16_t offset = current_block * BLOCK_SIZE_WORDS + block_offset;
        
        for(uint16_t i = 0; i < SEGMENT_WORDS; i++) {
          if(block_offset + i == BLOCK_SIZE_WORDS) {
            break;
          }
          
          __builtin_tblwtl((uint16_t)&_IGN_START + (offset + i) * 2, input->blockdata.data[i * 3] | input->blockdata.data[i * 3 + 1] << 8);
          __builtin_tblwth((uint16_t)&_IGN_START + (offset + i) * 2, input->blockdata.data[i * 3 + 2]);
        }
      }
    }
  }
}
