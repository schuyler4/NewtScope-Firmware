#include <stdint.h>

#include "pico/stdlib.h"
#include "hardware/spi.h"

#include "mcp41010.h"

#include "main.h"

static void chip_select_high(void)
{
    gpio_put(CS_PIN, 1);
}

static void chip_select_low(void)
{
    gpio_put(CS_PIN, 0);
}

void write_pot_code(uint8_t *code)
{
    uint8_t command = SET_WIPER_COMMAND;
    chip_select_low();  
    spi_write_blocking(spi1, &command, 1); 
    spi_write_blocking(spi1, code, 1); 
    chip_select_high();
}
