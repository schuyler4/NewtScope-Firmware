#ifndef SIMU_WAVEFORM_H
#define SIMU_WAVEFORM_H

#include <stdint.h>
#include <math.h>

#define PEAK_VOLTAGE 5
#define V_REF 1
#define OFFSET 1.75

#define ADC_BITS 8
#define RESOLUTION pow(2, ADC_BITS)
#define LSB V_REF/RESOLUTION

#define RESISTOR_LADDER 1000+499000+487000+8250
#define MULT_RESISTOR 8250

void simulate_waveform(uint16_t* signal, uint16_t point_count);

#endif
