#include "fifo.h"
#include "heap.h"
#include <stdlib.h>

struct FIFO_CTRL
{
    int r_ptr;    // 读取指针
    int w_ptr;    // 写入指针
    int crr_num;  // 当前存储的数据量
    int buf_size; // 环形缓冲区大小
    uint8_t *buf;
    uint8_t cover_flag; // 是否覆盖写入数据
    uint8_t heap_flag;
};

static void fifo_init(FIFO_CTRL_t *ctrl, uint8_t cover_flag)
{
    ctrl->r_ptr = 0;
    ctrl->w_ptr = 0;
    ctrl->crr_num = 0;
    ctrl->cover_flag = cover_flag;
    ctrl->heap_flag = 0;
    if (ctrl->buf == NULL && ctrl->buf_size > 0) // 不传入指定内存时，尝试开辟堆区
    {
        ctrl->buf = heap.api->malloc(heap.ctrl, ctrl->buf_size);
        if (ctrl->buf != NULL)
            ctrl->heap_flag = 1;
        else
            ctrl->buf_size = 0;
    }
}

static void fifo_free(FIFO_CTRL_t *ctrl)
{
    if (ctrl->heap_flag && ctrl->buf != NULL)
    {
        heap.api->free(heap.ctrl, ctrl->buf);
        ctrl->buf = NULL;
    }
    ctrl->r_ptr = 0;
    ctrl->w_ptr = 0;
    ctrl->crr_num = 0;
}

static uint8_t fifo_put(FIFO_CTRL_t *ctrl, void *p_data, uint16_t size)
{
    if (ctrl->buf_size <= 0 || p_data == NULL || size == 0)
        return 0;

    if (!ctrl->cover_flag && ctrl->crr_num + size > ctrl->buf_size)
        return 0; // 非覆盖模式下，内存不足写入直接返回

    for (uint16_t i = 0; i < size; i++)
    {
        if (ctrl->crr_num >= ctrl->buf_size)
        {
            ctrl->r_ptr++; // 强行踢出最早的数据
            ctrl->r_ptr %= ctrl->buf_size;
            ctrl->crr_num--;
        }

        ctrl->buf[ctrl->w_ptr] = ((uint8_t *)p_data)[i];
        ctrl->w_ptr++;
        ctrl->w_ptr %= ctrl->buf_size;
        ctrl->crr_num++;
    }

    return 1;
}

static uint8_t fifo_get(FIFO_CTRL_t *ctrl, void *const g_data, uint16_t size)
{
    if (ctrl->buf_size <= 0 || ctrl->crr_num < size || g_data == NULL || size == 0)
        return 0;

    for (uint16_t i = 0; i < size; i++)
    {
        ((uint8_t *)g_data)[i] = ctrl->buf[ctrl->r_ptr];
        ctrl->r_ptr++;
        ctrl->r_ptr %= ctrl->buf_size;
        ctrl->crr_num--;
    }

    return 1;
}

int fifo_get_crr_num(FIFO_CTRL_t *ctrl)
{
    return ctrl->crr_num;
}

//////////////////////////////////////////////

static uint8_t fifo_buf[256];

static FIFO_CTRL_t fifo_ctrl = {
    .r_ptr = 0,
    .w_ptr = 0,
    .crr_num = 0,
    .buf_size = sizeof(fifo_buf),
    .buf = fifo_buf,
    .cover_flag = 0,
    .heap_flag = 0,
};

static FIFO_API_t fifo_api = {
    .init = fifo_init,
    .free = fifo_free,
    .put = fifo_put,
    .get = fifo_get,
    .get_crr_num = fifo_get_crr_num,
};

const FIFO_t fifo = {
    .ctrl = &fifo_ctrl,
    .api = &fifo_api,
};
