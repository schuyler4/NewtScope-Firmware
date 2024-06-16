#ifndef MEMORY_H
#define MEMORY_H

#include <stdint.h>

#include "hardware/flash.h"

typedef struct
{
    uint16_t high_range_offset;
    uint16_t low_range_offset;
} Calibration_Offsets;

#define BUFFER_LENGTH FLASH_PAGE_SIZE/sizeof(uint16_t)
#define FLASH_TARGET_OFFSET (PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE)

#endif