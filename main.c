//
//  FILENAME: main.c
//
// TODO: description: This is the main program for Base Scope. It records samples,
// and stores frames using DMA. On a trigger condition, the trigger frame can be sent
// to a master processor. Additionally, the trigger level can be adjusted from the 
// master processor, and the master processor can assert a force trigger. Also,
// the attenuation of the analog front end is adjusted based on the scale selected
// by the master processor.  
//
// Written by Marek Newton with help from pico logic analyzer example.
//

#include <stdio.h>
#include <stdlib.h>

#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "hardware/structs/bus_ctrl.h"
#include "hardware/structs/pwm.h"

#include "led.pio.h"

#include "main.h"

#define PIN_COUNT 8
#define FIFO_REGISTER_WIDTH 32
#define PIN_BASE 16
#define SAMPLE_COUNT 65540
#define SAMPLE_FREQUENCY "125000000"

#define SPECS_COMMAND 's'
#define TRIGGER_COMMAND 't'
//130ns

int main(void)
{
    stdio_init_all();
    printf("Starting Sampler\n");
    bus_ctrl_hw->priority = BUSCTRL_BUS_PRIORITY_DMA_W_BITS | BUSCTRL_BUS_PRIORITY_DMA_R_BITS;
    
    uint total_sample_bits = SAMPLE_COUNT*PIN_COUNT;
    uint buffer_size_words = total_sample_bits/FIFO_REGISTER_WIDTH;
    uint32_t *capture_buffer = malloc(buffer_size_words*sizeof(uint32_t));
    hard_assert(capture_buffer);

    PIO pio = pio0;
    uint sm = 0;
    uint dma_channel = 0;

    sampler_init(pio, sm, PIN_BASE); 
    printf("Arming Trigger\n");
    arm_sampler(pio, sm, dma_channel, capture_buffer, buffer_size_words, PIN_BASE, true); 

    // PWM TEST -------------------------------------
    gpio_set_function(PIN_BASE, GPIO_FUNC_PWM);
    gpio_set_function(PIN_BASE + 1, GPIO_FUNC_PWM);
    // Topmost value of 3: count from 0 to 3 and then wrap, so period is 4 cycles
    pwm_hw->slice[0].top = 3;
    // Divide frequency by two to slow things down a little
    pwm_hw->slice[0].div = 4 << PWM_CH0_DIV_INT_LSB;
    // Set channel A to be high for 1 cycle each period (duty cycle 1/4) and
    // channel B for 3 cycles (duty cycle 3/4)
    pwm_hw->slice[0].cc =
         (1 << PWM_CH0_CC_A_LSB) |
         (3 << PWM_CH0_CC_B_LSB);
    // Enable this PWM slice
    pwm_hw->slice[0].csr = PWM_CH0_CSR_EN_BITS;

    // ----------------------------------------------
    dma_channel_wait_for_finish_blocking(dma_channel);

    while(1)
    {
        char command = (char)getchar();
        switch(command)
        {
            case SPECS_COMMAND:
                printf("START\n");
                printf("FS:%s\n", SAMPLE_FREQUENCY);        
                printf("END\n");
                break;
            case TRIGGER_COMMAND:
                printf("START\n");
                print_samples(capture_buffer, SAMPLE_COUNT);
                printf("END\n");
                break;
            default:
                // Do nothing
                break;
        }

    }

    // The program should never return. 
    return 1;
}

void run_led_program(PIO pio, uint sm, uint offset, uint freq)
{
    led_program_init(pio, sm, offset, PICO_DEFAULT_LED_PIN);
    pio_sm_set_enabled(pio, sm, true);
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
        capture_buffer, 
        &pio->rxf[sm],
        capture_size_words,
        true
    );
    
    pio_sm_exec(pio, sm, pio_encode_wait_gpio(trigger_level, trigger_pin));
    pio_sm_set_enabled(pio, sm, true);
}
  
void print_samples(uint32_t* sample_buffer, uint sample_buffer_length)
{
    uint32_t j;
    for(j = 0; j < PIN_COUNT; j++)
    {
        printf("PIN %d", j);
        uint32_t i;
        for(i = 0; i < sample_buffer_length; i++)
        {
            uint bit_index = j + i*PIN_COUNT;
            uint word_index = bit_index / FIFO_REGISTER_WIDTH;
            uint word_mask = 1 << bit_index % FIFO_REGISTER_WIDTH;
            if(sample_buffer[word_index] & word_mask)
            {
                printf("0");
            } 
            else
            {
                printf("1");
            }
        } 
        printf("\n");
    }
}
