#ifndef __DEBUG_H__
#define __DEBUG_H__

#include <stdio.h>

typedef struct DEBUG_CTRL DEBUG_CTRL_t;

typedef struct
{
    void (*init)(DEBUG_CTRL_t *);
    void (*printf)(DEBUG_CTRL_t *, const char *, ...);
    uint8_t (*scanf)(DEBUG_CTRL_t *, const char *, ...);
} DEBUG_API_t;

typedef struct
{
    DEBUG_CTRL_t *ctrl;
    DEBUG_API_t *api;
} DEBUG_t;

extern const DEBUG_t debug;

#endif
