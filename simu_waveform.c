#include <stdint.h>
#include <math.h>

#include "pico/stdlib.h"
#include "hardware/structs/pwm.h"

#include "main.h"

#include "simu_waveform.h"

static double attenuator(double vin)
{
    return (vin*MULT_RESISTOR)/RESISTOR_LADDER;
}

static uint16_t quantize_amplitude(double v)
{
    return (uint16_t)(v/LSB);
}

void simulate_waveform(uint16_t* signal, uint16_t point_count)
{
    uint16_t n = 0;
    for(n = 0; n < point_count; n++)
    {
        double v = attenuator(PEAK_VOLTAGE*sin((2*M_PI*n)/point_count)) + OFFSET;          
        *(signal + n) = quantize_amplitude(v); 
    }
}
