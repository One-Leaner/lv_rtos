#include "my_nes_port.h"
#include "InfoNES.h"
#include "InfoNES_System.h"
#include <stdio.h>
#include <string.h>

struct NES_CTRL
{
    volatile NES_STATE_t state;
    void *workframe;
    uint16_t height;
    uint16_t width;
};

/* 加载ROM并启动模拟器 */
static uint8_t nes_start(NES_CTRL_t *ctrl, const char *rom_path)
{
    /* 清空帧缓冲区 */
    if (ctrl->workframe != NULL)
    {
        memset(ctrl->workframe, 0, ctrl->width * ctrl->height * 2);
    }
    else
    {
        printf("NES: workframe is NULL\n");
        return 0;
    }

    /* 加载ROM */
    uint8_t res = InfoNES_Load(rom_path);
    if (res != 0)
    {
        printf("InfoNES: Failed to load ROM, res: %d\n", res);
        return 0;
    }
    InfoNES_Init();
    FrameSkip = 1;

    ctrl->state = NES_STATE_RUNNING;

    return 1;
}

// 刷新屏幕
static void nes_refresh(NES_CTRL_t *ctrl)
{
    if (ctrl->state != NES_STATE_RUNNING)
        return;

    InfoNES_Cycle();
}

/* 停止模拟器 */
static void nes_stop(NES_CTRL_t *ctrl)
{
    ctrl->state = NES_STATE_STOPPED;
    InfoNES_Fin();
}

/* 暂停模拟器 */
static void nes_pause(NES_CTRL_t *ctrl)
{
    ctrl->state = NES_STATE_PAUSED;
}

/* 恢复模拟器 */
static void nes_resume(NES_CTRL_t *ctrl)
{
    ctrl->state = NES_STATE_RUNNING;
}

/* 设置按键状态 */
static void nes_set_pad(NES_CTRL_t *ctrl, uint8_t pad1, uint8_t pad2)
{
    if (ctrl->state != NES_STATE_RUNNING)
        return;

    InfoNES_SetPadState(pad1, pad2, 0);
}

/* 获取工作缓冲区指针 */
static void *nes_get_workframe(NES_CTRL_t *ctrl)
{
    return ctrl->workframe;
}

uint16_t nes_get_height(NES_CTRL_t *ctrl)
{
    return ctrl->height;
}

uint16_t nes_get_width(NES_CTRL_t *ctrl)
{
    return ctrl->width;
}

NES_STATE_t nes_get_state(NES_CTRL_t *ctrl)
{
    return ctrl->state;
}

static NES_CTRL_t nes_ctrl = {
    .state = NES_STATE_STOPPED,
    .workframe = WorkFrame,
    .height = NES_DISP_HEIGHT,
    .width = NES_DISP_WIDTH,
};

static NES_API_t nes_api = {
    .start = nes_start,
    .refresh = nes_refresh,
    .pause = nes_pause,
    .resume = nes_resume,
    .stop = nes_stop,
    .set_pad = nes_set_pad,
    .get_workframe = nes_get_workframe,
    .get_height = nes_get_height,
    .get_width = nes_get_width,
    .get_state = nes_get_state,
};

/* 渲染器实例 */
const NES_t nes = {
    .ctrl = &nes_ctrl,
    .api = &nes_api,
};
