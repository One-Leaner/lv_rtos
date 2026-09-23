#include "buzzer.h"
#include "tim.h"

static const TIM_API_t *timx_api = &tim3_api;

void buzzer_set_volume(uint8_t volume)
{
    if (volume > 100)
        volume = 100;

    timx_api->set_duty(2, volume / 2);
}

void buzzer_on(void)
{
    timx_api->pwm_start(2);
}

void buzzer_off(void)
{
    timx_api->pwm_stop(2);
}