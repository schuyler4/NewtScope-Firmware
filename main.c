#include <pico/stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "pico/stdlib.h"
#include "pico/unique_id.h"

#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "hardware/structs/bus_ctrl.h"
#include "hardware/structs/pwm.h"
#include "hardware/spi.h"
#include "hardware/pwm.h"

#include "main.h"
#include "serial_protocol.h"
#include "mcp41010.h"

#define TEST_PIN 15

static uint8_t trigger_flag = 0;
static uint8_t sampler_created = 0;
static uint dma_channel = 0;
PIO sampler_pio = pio0;
uint sm = 0;
pio_sm_config c;

int main(void)
{
    stdio_init_all();

    setup_IO();
    setup_SPI();
    setup_cal_pin();

    bus_ctrl_hw->priority = BUSCTRL_BUS_PRIORITY_DMA_W_BITS | BUSCTRL_BUS_PRIORITY_DMA_R_BITS;
    clock_gpio_init(CLOCK_PIN, CLOCKS_CLK_GPOUT0_CTRL_AUXSRC_VALUE_CLK_SYS, 2); 
   
    uint8_t program_offset;
    TriggerType trigger_type = RISING_EDGE;
    c = pio_get_default_sm_config();

    while(1)
    {
        char command = (char)getchar();
        switch(command)
        {
            case HIGH_RANGE_COMMAND:
                gpio_put(RANGE_PIN, 0);
                break;
            case LOW_RANGE_COMMAND:
                gpio_put(RANGE_PIN, 1);
                break;
            case RISING_EDGE_TRIGGER_COMMAND:
                trigger_type = RISING_EDGE;
                break;
            case FALLING_EDGE_TRIGGER_COMMAND:
                trigger_type = FALLING_EDGE;
                break;
            case TRIGGER_COMMAND:
                {
                    if(!trigger_type)
                    {
                        trigger();
                        break;
                    }
                    else
                    {
                        trigger_flag = 1;
                    }
                    break;
                }
            case FORCE_TRIGGER_COMMAND:
                {
                    uint total_sample_bits = SAMPLE_COUNT*FORCE_TRIGGER_PIN_COUNT;
                    uint buffer_size_words = total_sample_bits/FIFO_REGISTER_WIDTH;
                    uint32_t *capture_buffer = malloc(buffer_size_words*sizeof(uint32_t));
                    hard_assert(capture_buffer);
                    if(!sampler_created)
                    {
                        sampler_init(&c, sampler_pio, sm, PIN_BASE, 1); 
                        sampler_created = 1;
                    }
                    arm_sampler(sampler_pio, sm, dma_channel, capture_buffer, buffer_size_words, PIN_BASE, 1, 1);
                    dma_channel_wait_for_finish_blocking(dma_channel);
                    print_samples(capture_buffer, SAMPLE_COUNT, 1);
                    free(capture_buffer);
                    break;
                }
            case TRIGGER_LEVEL_COMMAND:
                {
                    char code_string[MAX_STRING_LENGTH];
                    get_string(code_string);
                    uint8_t pot_code = atoi(code_string);
                    if(pot_code <= MAX_POT_CODE && pot_code >= MIN_POT_CODE)
                    {
                        write_pot_code(&pot_code);
                    }
                    break;
                }
            case CLOCK_DIV_COMMAND:
                {
                    char code_string[MAX_STRING_LENGTH];
                    get_string(code_string);
                    uint16_t clock_div = atoi(code_string);   
                    if(clock_div > 0)
                    {
                        sm_config_set_clkdiv(&c, clock_div);
                        pio_sm_set_config(sampler_pio, sm, &c);
                    } 
                    break;
                }    
            default:
                // Do nothing
                break;
        }
    }

    // The program should never return. 
    return 1;
}

void get_string(char* str)
{
    uint8_t i;
    for(i = 0; i < MAX_STRING_LENGTH; i++)
    {
        *(str + i) = (char)getchar();
        if(*(str + i) == '\0')
        {
            return;
        }
    }
}

void setup_IO(void)
{
    gpio_init(PS_SET_PIN);
    gpio_init(RANGE_PIN);
    gpio_init(CS_PIN);
    gpio_init(TRIGGER_PIN);
    
    gpio_set_dir(PS_SET_PIN, GPIO_OUT);
    gpio_set_dir(RANGE_PIN, GPIO_OUT);
    gpio_set_dir(CS_PIN, GPIO_OUT);
    gpio_set_dir(TRIGGER_PIN, GPIO_IN);

    gpio_put(PS_SET_PIN, 1); 
    gpio_put(RANGE_PIN, 0);
    gpio_put(CS_PIN, 1);

    gpio_set_function(CAL_PIN, GPIO_FUNC_PWM);
}

void setup_SPI(void)
{
    spi_init(spi1, SPI_SCK_FREQUENCY);
    gpio_set_function(SPI_SCK, GPIO_FUNC_SPI);
    gpio_set_function(SPI_RX, GPIO_FUNC_SPI);
    gpio_set_function(SPI_TX, GPIO_FUNC_SPI);
}

void setup_cal_pin(void)
{
    uint8_t slice_number = pwm_gpio_to_slice_num(CAL_PIN);
    pwm_set_chan_level(slice_number, PWM_CHAN_A, PWM_HIGH_COUNT);
    pwm_set_clkdiv(slice_number, PWM_CLK_DIV);
    pwm_set_enabled(slice_number, 1);
}

void trigger(void)
{
    uint total_sample_bits = SAMPLE_COUNT*FORCE_TRIGGER_PIN_COUNT;
    int buffer_size_words = total_sample_bits/FIFO_REGISTER_WIDTH;
    uint32_t *capture_buffer = malloc(buffer_size_words*sizeof(uint32_t));
    hard_assert(capture_buffer);
    if(!sampler_created)
    {
        sampler_init(&c, sampler_pio, sm, PIN_BASE, 0); 
        sampler_created = 1;
    }
    arm_sampler(sampler_pio, sm, dma_channel, capture_buffer, buffer_size_words, PIN_BASE, trigger_type, 0);
    dma_channel_wait_for_finish_blocking(dma_channel);
    print_samples(capture_buffer, SAMPLE_COUNT, 1);
    free(capture_buffer);
}

void trigger_callback(uint gpio, uint32_t event_mask)
{
    
}

uint8_t sampler_init(pio_sm_config* c, PIO pio, uint8_t sm, uint8_t pin_base, uint8_t force_trigger)
{
    uint8_t pin_count;
    if(!force_trigger)
    {
        pin_count = TRIGGER_PIN_COUNT;
    }
    else
    {
        pin_count = FORCE_TRIGGER_PIN_COUNT;
    }

    uint16_t sampling_instructions = pio_encode_in(pio_pins, pin_count);
    struct pio_program sample_prog = {
        .instructions = &sampling_instructions, 
        .length = 1, 
        .origin = -1
    };

    uint8_t offset = pio_add_program(pio, &sample_prog);
    sm_config_set_in_pins(c, pin_base);
    sm_config_set_wrap(c, offset, offset);
    sm_config_set_clkdiv(c, 2);
    sm_config_set_in_shift(c, true, true, FIFO_REGISTER_WIDTH);
    sm_config_set_fifo_join(c, PIO_FIFO_JOIN_RX);
    pio_sm_init(pio, sm, offset, c);
    return offset;
}

void arm_sampler(PIO pio, uint sm, uint dma_channel, uint32_t *capture_buffer, 
                size_t capture_size_words, uint trigger_pin, bool trigger_level, 
                uint8_t force_trigger)
{
    pio_sm_set_enabled(pio, sm, false);
    pio_sm_clear_fifos(pio, sm);
    pio_sm_restart(pio, sm);

    dma_channel_config c = dma_channel_get_default_config(dma_channel);
    channel_config_set_read_increment(&c, false);
    channel_config_set_write_increment(&c, true);
    channel_config_set_dreq(&c, pio_get_dreq(pio, sm, false));

    dma_channel_configure(dma_channel, &c,  capture_buffer, &pio->rxf[sm], capture_size_words, 1);
    
    // Used to trigger through the PIO
    if(!force_trigger)
    {
        pio_sm_exec(pio, sm, pio_encode_wait_gpio(trigger_level, TRIGGER_PIN));
    }
    pio_sm_set_enabled(pio, sm, true);
}

static inline uint bits_packed_per_word(uint pin_count) 
{
    const uint SHIFT_REG_WIDTH = 32;
    return SHIFT_REG_WIDTH - (SHIFT_REG_WIDTH % pin_count);
}
  
void print_samples(uint32_t* sample_buffer, uint sample_buffer_length, uint8_t force_trigger)
{
    uint8_t pin_count;
    if(force_trigger)
    {
        pin_count = FORCE_TRIGGER_PIN_COUNT;
    }
    else
    {
        pin_count = FORCE_TRIGGER_PIN_COUNT;
    }
    uint record_size_bits = bits_packed_per_word(pin_count);
    char samples[SAMPLE_COUNT];
    uint32_t j;
    for(j = 0; j < SAMPLE_COUNT; j++)
    {
        samples[j] = 0; 
    }
    uint8_t sample_pin_count;
    sample_pin_count = FORCE_TRIGGER_PIN_COUNT;
    for(j = 0; j < sample_pin_count; j++)
    {
        uint32_t i;
        for(i = 0; i < sample_buffer_length; i++)
        {
            uint bit_index = j + i * pin_count;
            uint word_index = bit_index / record_size_bits;
            uint word_mask = 1u << (bit_index % record_size_bits + 32 - record_size_bits);
            uint8_t bit = sample_buffer[word_index] & word_mask ? 1 : 0;
            samples[i] |= (bit << j);
        } 
    }
    write(1, samples, sizeof(samples));
}