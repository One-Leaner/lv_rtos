#ifndef __MATRIX_H__
#define __MATRIX_H__

typedef struct
{
    void (*identity)(float *, int);
    void (*multiply)(float *, int, int, float *, int, float *);
    void (*add)(float *, float *, int, int, float *);
    void (*reduce)(float *, float *, int, int, float *);
    void (*transpose)(float *, int, int, float *);
    uint8_t (*inverse_lu)(float *, int, float *);
    float (*gauss_deter)(float *, int);
    uint8_t (*gauss_elimination)(float *, float *, int, float *);
    void (*jacobi)(float *, int, float *, float *, int);
    void (*printf)(float *, int, int);
} MATRIX_API_t;

extern const MATRIX_API_t matrix_api;

#endif
