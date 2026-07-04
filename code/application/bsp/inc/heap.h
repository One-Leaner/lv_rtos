#ifndef __HEAP_H__
#define __HEAP_H__

#include <stdint.h>

typedef struct HEAP_CTRL HEAP_CTRL_t;

typedef struct
{
    void (*init)(HEAP_CTRL_t *);
    void *(*malloc)(HEAP_CTRL_t *, uint32_t);
    void (*free)(HEAP_CTRL_t *, void *const);
    int (*get_free_size)(HEAP_CTRL_t *ctrl);
} HEAP_API_t;

// Á´±í
typedef struct
{
    HEAP_CTRL_t *ctrl;
    HEAP_API_t *api;
} HEAP_t;

extern const HEAP_t heap;

#endif
