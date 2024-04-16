//
//  FILENAME: main.c
//
// TODO: description: This is the main program for Newt Scope. It records samples,
// and stores frames using DMA. On a trigger condition, the trigger frame can be sent
// to a master processor. Additionally, the trigger level can be adjusted from the 
// master processor, and the master processor can assert a force trigger. Also,
// the attenuation of the analog front end is adjusted based on the scale selected
// by the master processor.  
//
// Written by Marek Newton.
//

#include <pico/stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "hardware/structs/bus_ctrl.h"
#include "hardware/structs/pwm.h"
#include "hardware/spi.h"

#include "main.h"
#include "simu_waveform.h"
#include "serial_protocol.h"
#include "mcp41010.h"

#define TEST_PIN 15

static uint8_t trigger_flag = 0;

int main(void)
{
    stdio_init_all();
    setup_IO();
    setup_SPI();
    bus_ctrl_hw->priority = BUSCTRL_BUS_PRIORITY_DMA_W_BITS | BUSCTRL_BUS_PRIORITY_DMA_R_BITS;
    clock_gpio_init(CLOCK_PIN, CLOCKS_CLK_GPOUT0_CTRL_AUXSRC_VALUE_CLK_SYS, 1);
   
    uint total_sample_bits = SAMPLE_COUNT*PIN_COUNT;
    uint buffer_size_words = total_sample_bits/FIFO_REGISTER_WIDTH;
    uint32_t *capture_buffer = malloc(buffer_size_words*sizeof(uint32_t));
    hard_assert(capture_buffer);

    PIO sampler_pio = pio0;
    uint sm = 0;
    uint dma_channel = 0;

    sampler_init(sampler_pio, sm, PIN_BASE); 
    while(1)
    {
        char command = (char)getchar();
        switch(command)
        {
            case SPECS_COMMAND:
                break;
            case HIGH_RANGE_COMMAND:
                gpio_put(RANGE_PIN, 0);
                break;
            case LOW_RANGE_COMMAND:
                gpio_put(RANGE_PIN, 1);
                break;
            case TRIGGER_COMMAND:
                printf(START_COMMAND);
                arm_sampler(sampler_pio, 
                    sm, dma_channel, capture_buffer, buffer_size_words, PIN_BASE, true);
                dma_channel_wait_for_finish_blocking(dma_channel);
                print_samples(capture_buffer, SAMPLE_COUNT);
                printf(END_COMMAND);
                break;
            case FORCE_TRIGGER_COMMAND:
                break;
            case SIMU_TRIGGER_COMMAND:
                {
                    uint16_t xx[SIMU_WAVEFORM_POINTS];
                    simulate_waveform(xx, SIMU_WAVEFORM_POINTS);
                    transmit_vector(xx, SIMU_WAVEFORM_POINTS);
                    break; 
                }
            case TRIGGER_LEVEL_COMMAND:
                {
                    uint8_t pot_code = (uint8_t)getchar();
                    if(pot_code <= MAX_POT_CODE && pot_code >= MIN_POT_CODE)
                    {
                        write_pot_code(&pot_code);
                    }
                }
                break;
            default:
                // Do nothing
                break;
        }
    }

    // The program should never return. 
    return 1;
}

void setup_IO(void)
{
    gpio_init(PS_SET_PIN);
    gpio_init(TRIGGER_PIN);
    gpio_init(RANGE_PIN);
    gpio_init(CS_PIN);
    
    gpio_set_dir(TRIGGER_PIN, GPIO_IN);
    gpio_set_dir(PS_SET_PIN, GPIO_OUT);
    gpio_set_dir(RANGE_PIN, GPIO_OUT);
    gpio_set_dir(CS_PIN, GPIO_OUT);

    gpio_put(PS_SET_PIN, 1); 
    gpio_put(RANGE_PIN, 0);
    gpio_put(CS_PIN, 1);
    
    gpio_set_irq_enabled_with_callback(TRIGGER_PIN, GPIO_IRQ_EDGE_RISE, 1, trigger_callback);
}

void setup_SPI(void)
{
    spi_init(spi1, SPI_SCK_FREQUENCY);
    gpio_set_function(SPI_SCK, GPIO_FUNC_SPI);
    gpio_set_function(SPI_RX, GPIO_FUNC_SPI);
    gpio_set_function(SPI_TX, GPIO_FUNC_SPI);
}

void sampler_init(PIO pio, uint sm, uint pin_base)
{
    uint16_t sampling_instructions = pio_encode_in(pio_pins, PIN_COUNT);
    struct pio_program sample_prog = {
        .instructions = &sampling_instructions, 
        .length = 1, 
        .origin = -1
    };
    uint offset = pio_add_program(pio, &sample_prog);
    pio_sm_config c = pio_get_default_sm_config();

    sm_config_set_in_pins(&c, pin_base);
    sm_config_set_wrap(&c, offset, offset);
    sm_config_set_clkdiv(&c, 1);
    sm_config_set_in_shift(&c, true, true, FIFO_REGISTER_WIDTH);
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_RX);
    pio_sm_init(pio, sm, offset, &c);
}

void arm_sampler(PIO pio, uint sm, uint dma_channel, uint32_t *capture_buffer, 
    size_t capture_size_words, uint trigger_pin, bool trigger_level)
{
    pio_sm_set_enabled(pio, sm, false);
    pio_sm_clear_fifos(pio, sm);

    dma_channel_config c = dma_channel_get_default_config(dma_channel);
    channel_config_set_read_increment(&c, false);
    channel_config_set_write_increment(&c, true);
    channel_config_set_dreq(&c, pio_get_dreq(pio, sm, false));

    dma_channel_configure(dma_channel, &c, 
        capture_buffer, &pio->rxf[sm],
        capture_size_words, true
    );
    
    // Used to trigger through the PIO
    // pio_sm_exec(pio, sm, pio_encode_wait_gpio(trigger_level, BASE_PIN));
    pio_sm_set_enabled(pio, sm, true);
}

void trigger_callback(uint gpio, uint32_t event_mask)
{
    trigger_flag = 1;
}

void transmit_vector(uint16_t* vector, uint16_t point_count)
{
    printf(START_COMMAND);
    uint32_t n;
    for(n = 0; n < point_count; n++)
    {
        printf("%d\n", vector[n]);
    }
    printf(END_COMMAND);
}

static inline uint bits_packed_per_word(uint pin_count) 
{
    const uint SHIFT_REG_WIDTH = 32;
    return SHIFT_REG_WIDTH - (SHIFT_REG_WIDTH % pin_count);
}
  
void print_samples(uint32_t* sample_buffer, uint sample_buffer_length)
{
    uint record_size_bits = bits_packed_per_word(PIN_COUNT);
    uint32_t samples[SAMPLE_COUNT];
    uint32_t j;
    for(j = 0; j < SAMPLE_COUNT; j++)
    {
        samples[j] = 0; 
    }
    for(j = 0; j < PIN_COUNT; j++)
    {
        uint32_t i;
        printf("%d\n", j);
        for(i = 0; i < sample_buffer_length; i++)
        {
            uint bit_index = j + i * PIN_COUNT;
            uint word_index = bit_index / record_size_bits;
            uint word_mask = 1u << (bit_index % record_size_bits + 32 - record_size_bits);
            uint8_t bit = sample_buffer[word_index] & word_mask ? 1 : 0;
            samples[i] |= (bit << j);
        } 
        printf("\n");
    }
    for(j = 0; j < SAMPLE_COUNT; j++)
    {
        printf("%d\n", samples[j]); 
    }
}
