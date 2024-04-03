#include "pico/stdlib.h"
#include "hardware/structs/pwm.h"

#include "main.h"

void simulate_waveform(void)
{
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
}
