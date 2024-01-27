//
// FILENAME: main.h
//
// description: Header file for main.c.
//
// Written by Marek Newton
//

#ifndef MAIN_H
#define MAIN_H

void run_led_program(PIO pio, uint sm, uint offset, uint freq);
void sampler_init(PIO pio, uint sm, uint pin_base);
void arm_sampler(PIO pio, uint sm, uint dma_channel, uint32_t *capture_buffer, 
size_t capture_size_words, uint trigger_pin, bool trigger_level);

#endif
