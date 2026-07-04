#include "buzzer.h"
#include "tim.h"

struct BUZZER_CTRL
{
    const TIM_API_t *timx_api;
};

// 0-100
static void buzzer_set_volume(BUZZER_CTRL_t *ctrl, uint8_t volume)
{
    if (volume > 100)
        volume = 100;

    ctrl->timx_api->set_duty(2, volume / 2);
}

static void buzzer_on(BUZZER_CTRL_t *ctrl)
{
    ctrl->timx_api->pwm_start(2);
}

static void buzzer_off(BUZZER_CTRL_t *ctrl)
{
    ctrl->timx_api->pwm_stop(2);
}

static BUZZER_CTRL_t buzzer_ctrl = {
    .timx_api = &tim3_api,
};

static BUZZER_API_t buzzer_api = {
    .set_volume = buzzer_set_volume,
    .on = buzzer_on,
    .off = buzzer_off,
};

const BUZZER_t buzzer = {
    .ctrl = &buzzer_ctrl,
    .api = &buzzer_api,
};