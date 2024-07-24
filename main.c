#include <pico/stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "pico/stdlib.h"
#include "pico/unique_id.h"

#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "hardware/structs/bus_ctrl.h"
#include "hardware/structs/pwm.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include "hardware/irq.h"
#include "hardware/flash.h"
#include "hardware/sync.h"

#include "main.h"
#include "serial_protocol.h"
#include "calibration_memory.h"
#include "mcp41010.h"
#include "normal_capture.pio.h"

#define TEST_PIN 15

static uint8_t trigger_flag = 0;
static uint8_t sampler_created = 0;

static uint dma_channel = 0;
static uint positive_dma_channel = 0;
static uint negative_dma_channel_one = 1;
static uint negative_dma_channel_two = 2;

PIO auto_sampler_pio = pio0;

PIO negative_sampler_pio = pio0;
PIO positive_sampler_pio = pio1;

uint negative_sampler_sm = 0;
uint positive_sampler_sm = 1;
uint auto_sampler_sm = 0;

pio_sm_config auto_sampler_config;
pio_sm_config negative_sampler_config;
pio_sm_config positive_sampler_config;

dma_channel_config negative_dma_config_one;
dma_channel_config negative_dma_config_two;
dma_channel_config positive_dma_config;

uint negative_sampler_offset;
uint positive_sampler_offset;

uint32_t *negative_capture_buffer_one;
uint32_t *negative_capture_buffer_two;
uint32_t *positive_capture_buffer;
uint32_t *capture_buffer;

TriggerType trigger_type = RISING_EDGE;
uint8_t force_trigger = 0;
uint8_t normal_trigger = 0;
uint8_t sampler_clock_div = 1;
uint8_t normal_sampler_created = 0;

volatile uint8_t trigger_vector_available = 0;

int main(void)
{
    stdio_init_all();

    setup_IO();
    setup_SPI();
    setup_cal_pin();

    bus_ctrl_hw->priority = BUSCTRL_BUS_PRIORITY_DMA_W_BITS | BUSCTRL_BUS_PRIORITY_DMA_R_BITS;
    clock_gpio_init(CLOCK_PIN, CLOCKS_CLK_GPOUT0_CTRL_AUXSRC_VALUE_CLK_SYS, 2); 
   
    auto_sampler_config = pio_get_default_sm_config();

    while(1)
    {
        if(trigger_vector_available)
        {
            //teardown_normal_sampler(negative_sampler_pio, positive_sampler_pio, negative_sampler_sm, positive_sampler_sm, 
            //                    negative_dma_channel, positive_dma_channel, negative_sampler_offset, positive_sampler_offset);
            print_samples(negative_capture_buffer_one, SAMPLE_COUNT, force_trigger);
            free(positive_capture_buffer);
            free(negative_capture_buffer_one);
            free(negative_capture_buffer_two);
            trigger_vector_available = 0;
        }

        if(trigger_flag && !gpio_get(TRIGGER_PIN))
        {
            trigger(false);
            trigger_flag = 0;
        }

        char command = (char)getchar_timeout_us(CHARACTER_TIMEOUT);
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
                    reset_triggers();
                    run_trigger();
                    normal_trigger = 1;
                    break;
                }
            case FORCE_TRIGGER_COMMAND:
                {
                    reset_triggers();
                    force_trigger = 1;
                    trigger(force_trigger);
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
                    sampler_clock_div = atoi(code_string);   
                    /*
                    if(clock_div > 0)
                    {
                        if(normal_trigger)
                        {
                            sm_config_set_clkdiv(&normal_sampler_config, clock_div*2);
                            pio_sm_set_config(normal_sampler_pio, normal_sampler_sm, &normal_sampler_config);
                        }
                        else
                        {
                            sm_config_set_clkdiv(&auto_sampler_config, clock_div*2);
                            pio_sm_set_config(auto_sampler_pio, auto_sampler_sm, &auto_sampler_config);
                        }
                    } 
                    */
                    break;
                }    
            case STOP_COMMAND:
                {
                    uint32_t interrupt_state = save_and_disable_interrupts();
                    pio_sm_set_enabled(auto_sampler_pio, auto_sampler_sm, false);
                    dma_channel_abort(dma_channel);
                    irq_set_enabled(DMA_IRQ_0, false);
                    reset_triggers();
                    restore_interrupts(interrupt_state);
                    break;
                }
            case SET_CAL:
                {
                    char cal_string[MAX_STRING_LENGTH];
                    get_string(cal_string);
                    char* high_range_cal_string = malloc(4*sizeof(char));   
                    char* low_range_cal_string = malloc(4*sizeof(char));
                    memcpy(high_range_cal_string, cal_string, 4*sizeof(char));
                    memcpy(low_range_cal_string, cal_string+4, 4*sizeof(char));
                    uint16_t high_range_cal = atoi(high_range_cal_string);
                    uint16_t low_range_cal = atoi(low_range_cal_string);
                    Calibration_Offsets calibration_offsets; 
                    calibration_offsets.high_range_offset = high_range_cal;
                    calibration_offsets.low_range_offset = low_range_cal;
                    write_calibration_offsets(calibration_offsets);
                    free(high_range_cal_string);
                    free(low_range_cal_string);
                    break;
                }
            case READ_CAL:
                {
                    Calibration_Offsets calibration_offsets = read_calibration_offsets();
                    uint8_t low_high_range_byte = (uint8_t)(calibration_offsets.high_range_offset & 0xFF);
                    uint8_t high_high_range_byte = (uint8_t)((calibration_offsets.high_range_offset >> 8) & 0xFF);
                    uint8_t low_low_range_byte = (uint8_t)(calibration_offsets.low_range_offset & 0xFF);
                    uint8_t high_low_range_byte = (uint8_t)((calibration_offsets.low_range_offset >> 8) & 0xFF);
                    write(1, &low_high_range_byte, sizeof(uint8_t));
                    write(1, &high_high_range_byte, sizeof(uint8_t));
                    write(1, &low_low_range_byte, sizeof(uint8_t));
                    write(1, &high_low_range_byte, sizeof(uint8_t)); 
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

void reset_triggers(void)
{
    trigger_vector_available = 0;
    trigger_flag = 0;
    dma_hw->ints0 = 1 << dma_channel;
}

void run_trigger(void)
{
    force_trigger = 0;
    if(!gpio_get(TRIGGER_PIN))
    {
        trigger(false);
    }
    else
    {
        if(!trigger_flag)
        {
            trigger_flag = 1;
        }
    }
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

void dma_complete_handler(void)
{
    printf("IN INTERRUPT");
    trigger_vector_available = 1; 
    dma_hw->ints0 = 1 << dma_channel;
    if(normal_trigger) 
    {
    }
}

uint8_t auto_sampler_init(pio_sm_config* c, PIO pio, uint8_t sm, uint8_t pin_base)
{
    uint16_t sampling_instructions = pio_encode_in(pio_pins, FORCE_TRIGGER_PIN_COUNT);
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
    dma_channel_set_irq0_enabled(dma_channel, true);
    irq_set_exclusive_handler(DMA_IRQ_0, dma_complete_handler);
    irq_set_enabled(DMA_IRQ_0, true);
    
    pio_sm_set_enabled(pio, sm, true);
}

void arm_normal_sampler(PIO negative_pio, PIO positive_pio, uint positive_sm, uint negative_sm, 
                        uint negative_dma_channel, uint positive_dma_channel, 
                        uint32_t *negative_capture_buffer, 
                        uint32_t *positive_capture_buffer, size_t capture_size_words, uint trigger_pin)
{
    pio_sm_set_enabled(negative_pio, negative_sm, false);
    pio_sm_clear_fifos(negative_pio, negative_sm);
    pio_sm_restart(negative_pio, negative_sm);

    pio_sm_set_enabled(positive_pio, positive_sm, false);
    pio_sm_clear_fifos(positive_pio, positive_sm);
    pio_sm_restart(positive_pio, positive_sm);

    negative_dma_config_one = dma_channel_get_default_config(negative_dma_channel_one);
    channel_config_set_read_increment(&negative_dma_config_one, false);
    channel_config_set_write_increment(&negative_dma_config_one, true);
    channel_config_set_chain_to(&negative_dma_config_one, positive_dma_channel);
    channel_config_set_dreq(&negative_dma_config_one, pio_get_dreq(negative_pio, negative_sm, false));
    //channel_config_set_ring(&negative_dma_config_one, true, 14);

    //negative_dma_config_two = dma_channel_get_default_config(negative_dma_channel_two);
    //channel_config_set_read_increment(&negative_dma_config_two, false);
    //channel_config_set_write_increment(&negative_dma_config_two, true);
    //channel_config_set_chain_to(&negative_dma_config_two, negative_dma_channel_one);
    //channel_config_set_dreq(&negative_dma_config_two, pio_get_dreq(negative_pio, negative_sm, false));
    //channel_config_set_ring(&negative_dma_config_two, true, 14);

    positive_dma_config = dma_channel_get_default_config(positive_dma_channel);
    channel_config_set_read_increment(&positive_dma_config, false);
    channel_config_set_write_increment(&positive_dma_config, true);
    channel_config_set_dreq(&positive_dma_config, pio_get_dreq(positive_pio, positive_sm, false));

    dma_channel_set_irq0_enabled(positive_dma_channel, true);

    //pio_interrupt_clear(pio, 0);
    //pio_set_irq0_source_enabled(pio, pis_interrupt0, true);
    irq_set_exclusive_handler(DMA_IRQ_0, dma_complete_handler);
    //irq_set_enabled(pio_get_dreq(pio, sm, false), true);
    
    // Enable both the PIOs
    pio_sm_set_enabled(negative_pio, negative_sm, true);
    pio_sm_set_enabled(positive_pio, positive_sm, true);

    // Send half the sample buffer length to the negative state machine
    pio_sm_put_blocking(negative_pio, negative_sm, (SAMPLE_COUNT/2)-1);

    dma_channel_configure(positive_dma_channel, &positive_dma_config, positive_capture_buffer, 
                        &positive_pio->rxf[positive_sm], capture_size_words, true);
    dma_channel_configure(negative_dma_channel_one, &negative_dma_config_one, negative_capture_buffer_one, 
                        &negative_pio->rxf[negative_sm], capture_size_words, false);
    //dma_channel_configure(negative_dma_channel_two, &negative_dma_config_two, negative_capture_buffer_two, 
    //                    &negative_pio->rxf[negative_sm], capture_size_words, false);
    irq_set_enabled(DMA_IRQ_0, true);
    printf("Made it here\n");
}

void teardown_normal_sampler(PIO negative_pio, PIO positive_pio, uint negative_sm, uint positive_sm, 
                             uint negative_dma_channel, uint positive_dma_channel, uint negative_offset, 
                             uint positive_offset)
{
    // Teardown the negative channel.
    dma_channel_abort(negative_dma_channel);
    dma_channel_unclaim(negative_dma_channel);
    pio_sm_set_enabled(negative_pio, negative_sm, false);
    pio_sm_unclaim(negative_pio, negative_sm);
    pio_remove_program(negative_pio, &negative_capture_program, negative_offset);

    // Teardown the positive channel.
    //irq_set_enabled(DMA_IRQ_0, false);
    //dma_channel_abort(positive_dma_channel);
    //dma_channel_unclaim(positive_dma_channel);
    //pio_sm_set_enabled(positive_pio, positive_sm, false);
    //pio_sm_unclaim(positive_pio, positive_sm);
    //pio_remove_program(positive_pio, &positive_capture_program, positive_offset);
}

void trigger(uint8_t forced)
{
    if(forced)
    {
        uint total_sample_bits = SAMPLE_COUNT*FORCE_TRIGGER_PIN_COUNT;
        int buffer_size_words = total_sample_bits/FIFO_REGISTER_WIDTH;
        capture_buffer = malloc(buffer_size_words*sizeof(uint32_t));
        if(!sampler_created)
        {
            auto_sampler_init(&auto_sampler_config, auto_sampler_pio, auto_sampler_sm, PIN_BASE); 
            sampler_created = 1;
        }
        arm_sampler(auto_sampler_pio, auto_sampler_sm, dma_channel, capture_buffer, buffer_size_words, PIN_BASE, trigger_type, forced);
    }
    else
    {
        uint total_sample_bits = SAMPLE_COUNT*FORCE_TRIGGER_PIN_COUNT;
        int buffer_size_words = total_sample_bits/FIFO_REGISTER_WIDTH;
        negative_capture_buffer_one = malloc(buffer_size_words*sizeof(uint32_t));
        negative_capture_buffer_two = malloc(buffer_size_words*sizeof(uint32_t));
        positive_capture_buffer = malloc(buffer_size_words*sizeof(uint32_t));
        if(!normal_sampler_created)
        {
            normal_sampler_created = 1;
            negative_sampler_offset = pio_add_program(negative_sampler_pio, &negative_capture_program);
            positive_sampler_offset = pio_add_program(positive_sampler_pio, &positive_capture_program);
            negative_sampler_init(negative_sampler_pio, negative_sampler_sm, negative_sampler_offset, PIN_BASE, 
                                  TRIGGER_PIN, sampler_clock_div);
            positive_sampler_init(positive_sampler_pio, positive_sampler_sm, positive_sampler_offset, PIN_BASE, 
                                  TRIGGER_PIN, sampler_clock_div);
        }
        arm_normal_sampler(negative_sampler_pio, positive_sampler_pio, positive_sampler_sm, negative_sampler_sm, 
                        negative_dma_channel_one, positive_dma_channel, 
                        negative_capture_buffer_one, 
                        positive_capture_buffer, buffer_size_words, TRIGGER_PIN);
    }
}

static inline uint bits_packed_per_word(uint pin_count) 
{
    const uint SHIFT_REG_WIDTH = 32;
    return SHIFT_REG_WIDTH - (SHIFT_REG_WIDTH % pin_count);
}
 
void print_samples(uint32_t* sample_buffer, uint sample_buffer_length, uint8_t force_trigger)
{
    uint record_size_bits = bits_packed_per_word(FORCE_TRIGGER_PIN_COUNT);
    char samples[SAMPLE_COUNT];
    uint32_t j;
    for(j = 0; j < SAMPLE_COUNT; j++)
    {
        samples[j] = 0; 
    }
    for(j = 0; j < FORCE_TRIGGER_PIN_COUNT; j++)
    {
        uint32_t i;
        for(i = 0; i < sample_buffer_length; i++)
        {
            uint bit_index = j + i * FORCE_TRIGGER_PIN_COUNT;
            uint word_index = bit_index / record_size_bits;
            uint word_mask = 1u << (bit_index % record_size_bits + 32 - record_size_bits);
            uint8_t bit = sample_buffer[word_index] & word_mask ? 1 : 0;
            samples[i] |= (bit << j);
        } 
    }
    write(1, samples, sizeof(samples));
}