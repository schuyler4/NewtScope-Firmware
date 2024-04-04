#ifndef MAIN_H
#define MAIN_H

#define PIN_COUNT 8
#define FIFO_REGISTER_WIDTH 32
#define PIN_BASE 0
#define SAMPLE_COUNT 500
#define SAMPLE_FREQUENCY "125000000"

#define TRIGGER_PIN 8

#define SIMU_WAVEFORM_POINTS 50

#include "pico/stdlib.h"
#include "hardware/pio.h"

void setup_IO(void);
void sampler_init(PIO pio, uint sm, uint pin_base);
void arm_sampler(PIO pio, uint sm, uint dma_channel, uint32_t *capture_buffer, 
    size_t capture_size_words, uint trigger_pin, bool trigger_level);
void trigger_callback(uint gpio, uint32_t event_mask);
void print_samples(uint32_t* sample_buffer, uint sample_buffer_length);
void transmit_vector(uint16_t* vector, uint16_t point_count);

#endif
