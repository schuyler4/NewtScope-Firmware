#include "hardware/flash.h"

#include "calibration_memory.h"

void write_calibration_offsets(Calibration_Offsets calibration_offsets)
{
    uint8_t buffer[BUFFER_LENGTH];
    buffer[0] = calibration_offsets.high_range_offset >> 8;
    buffer[1] = calibration_offsets.high_range_offset & 0xFF;
    buffer[2] = calibration_offsets.low_range_offset >> 8;
    buffer[3] = calibration_offsets.low_range_offset & 0xFF;
    flash_range_erase((PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE), FLASH_SECTOR_SIZE);
    flash_range_program((PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE), (uint8_t*)buffer, FLASH_PAGE_SIZE);
}

Calibration_Offsets read_calibration_offsets(void)
{
    uint16_t* address = (uint16_t*)(XIP_BASE + FLASH_TARGET_OFFSET);
    Calibration_Offsets calibration_offsets;
    uint8_t high_range_high_byte = *(address + 0);
    uint8_t high_range_low_byte = *(address + 1);
    uint8_t low_range_high_byte = *(address + 2);
    uint8_t low_range_low_byte = *(address + 3);
    calibration_offsets.high_range_offset = (high_range_high_byte << 8) | high_range_low_byte;
    calibration_offsets.low_range_offset = (low_range_high_byte << 8) | low_range_low_byte;
    return calibration_offsets;
}