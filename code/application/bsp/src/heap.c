#include "heap.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stdlib.h>

#define HEAP_CUSTOM_HEAP 0 // 是否使用自定义内存管理

// 节点
typedef struct HEAP_NODE
{
    void *heap_addr;
    uint32_t size;
    struct HEAP_NODE *last;
    struct HEAP_NODE *next;
    uint8_t idle_flag;
} HEAP_NODE_t;

struct HEAP_CTRL
{
    void *const pool;          // 内存池
    const uint32_t total_size; // 总大小
    HEAP_NODE_t *head;         // 头指针
};

static void heap_init(HEAP_CTRL_t *ctrl)
{
#if !HEAP_CUSTOM_HEAP
    return;
#endif

    // 创建头节点
    ctrl->head = (HEAP_NODE_t *)ctrl->pool;
    ctrl->head->heap_addr = (uint8_t *)(ctrl->pool) + sizeof(HEAP_NODE_t);
    ctrl->head->size = ctrl->total_size - sizeof(HEAP_NODE_t);
    ctrl->head->last = NULL;
    ctrl->head->next = NULL;
    ctrl->head->idle_flag = 1;
}

static void *heap_malloc(HEAP_CTRL_t *ctrl, uint32_t size)
{
#if !HEAP_CUSTOM_HEAP
    // return malloc(size);
    return pvPortMalloc(size);
#endif

    if (size == 0)
        return NULL;

    size = (size + 3) & ~3u; // 4字节内存对齐

    HEAP_NODE_t *crr = ctrl->head;
    HEAP_NODE_t *best = NULL;
    uint32_t min_size = 0xffffffff;

    while (crr != NULL)
    {
        if (crr->idle_flag && crr->size >= size)
        {
            if (min_size >= crr->size) // 选择最小适配的那块
            {
                best = crr;
                min_size = crr->size;
            }
        }
        crr = crr->next; // 下一块空闲区
    }

    if (best != NULL)
    {
        // 分割节点（剩余内存大于一个节点头时才分割，否则全部分配给当前对象->避免内存碎片，
        // 但是申请的内存会大一点，最大超出一个节点大小）
        if (best->size - size > sizeof(HEAP_NODE_t))
        {
            uint8_t *new_node_addr = (uint8_t *)(best->heap_addr) + size;
            uint32_t new_idle_size = best->size - size - sizeof(HEAP_NODE_t);

            HEAP_NODE_t *new_node = (HEAP_NODE_t *)new_node_addr;
            new_node->heap_addr = (uint8_t *)new_node + sizeof(HEAP_NODE_t);
            new_node->size = new_idle_size;
            new_node->next = best->next;
            new_node->last = best;
            new_node->idle_flag = 1;

            if (new_node->next != NULL)
                best->next->last = new_node;
            best->next = new_node;
            best->size = size;
        }

        best->idle_flag = 0; // 标记已使用

        // 移动头指针
        if (best == ctrl->head)
        {
            while (ctrl->head != NULL && !ctrl->head->idle_flag)
            {
                ctrl->head = ctrl->head->next;
            }
        }

        return best->heap_addr;
    }

    return NULL;
}

static void heap_free(HEAP_CTRL_t *ctrl, void *const addr)
{
#if !HEAP_CUSTOM_HEAP
    // free(addr);
    vPortFree(addr);
    return;
#endif

    if (addr == NULL || ctrl == NULL)
        return;

    HEAP_NODE_t *node = (HEAP_NODE_t *)((uint8_t *)addr - sizeof(HEAP_NODE_t));
    if (node->idle_flag)
        return;

    // 合并相邻空闲块
    if (node->next != NULL && node->next->idle_flag) // 向后合并
    {
        uint8_t *next_start_addr = (uint8_t *)node + sizeof(HEAP_NODE_t) + node->size;
        if (node->next == (HEAP_NODE_t *)next_start_addr) // 物理地址连续才进行合并
        {
            node->size += node->next->size + sizeof(HEAP_NODE_t);
            node->next = node->next->next;
            if (node->next != NULL)
                node->next->last = node;
        }
    }
    if (node->last != NULL && node->last->idle_flag) // 向前合并
    {
        HEAP_NODE_t *last_node = node->last;
        uint8_t *last_end_addr = (uint8_t *)last_node + sizeof(HEAP_NODE_t) + last_node->size;
        if (node == (HEAP_NODE_t *)last_end_addr)
        {
            last_node->size += sizeof(HEAP_NODE_t) + node->size;
            last_node->next = node->next;
            if (node->next != NULL)
                node->next->last = last_node;

            node = last_node;
        }
    }
    node->idle_flag = 1; // 标记为空闲

    // 移动头指针
    if (node->last == NULL || !node->last->idle_flag)
    {
        ctrl->head = node;
    }
}

static int heap_get_free_size(HEAP_CTRL_t *ctrl)
{
    if (ctrl == NULL)
        return 0;

    int free_size = 0;
    HEAP_NODE_t *crr = ctrl->head;
    while (crr != NULL)
    {
        if (crr->idle_flag)
        {
            free_size += crr->size + sizeof(HEAP_NODE_t); // 可用大小=空闲块大小+节点头大小
        }
        crr = crr->next;
    }
    return free_size;
}

/////////////////////////////////////////////////////////////////////
#if HEAP_CUSTOM_HEAP
static uint8_t heap_pool[16 * 1024]; // 内存池
#endif

HEAP_CTRL_t heap_ctrl = {
#if HEAP_CUSTOM_HEAP
    .pool = heap_pool,
    .total_size = sizeof(heap_pool),
#else
    .pool = NULL,
    .total_size = 0,
#endif
    .head = NULL,
};

HEAP_API_t heap_api = {
    .init = heap_init,
    .malloc = heap_malloc,
    .free = heap_free,
    .get_free_size = heap_get_free_size,
};

const HEAP_t heap = {
    .ctrl = &heap_ctrl,
    .api = &heap_api,
};
