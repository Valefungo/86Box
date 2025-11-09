#ifndef __86BOX_ATOMICS
#define __86BOX_ATOMICS

// #include <stdatomic.h>
#include <stdio.h>

#define _Atomic

typedef _Bool atomic_bool;
typedef char atomic_char;
typedef signed char atomic_schar;
typedef unsigned char atomic_uchar;
typedef short atomic_short;
typedef unsigned short atomic_ushort;
typedef int atomic_int;
typedef unsigned int atomic_uint;
typedef long atomic_long;
typedef unsigned long atomic_ulong;
typedef long long atomic_llong;
typedef unsigned long long atomic_ullong;

typedef char atomic_flag;

// #define atomic_init(a, b) *a=b
#define atomic_load(a) *a
// #define atomic_store(a, b) *a=b
// #define atomic_fetch_add(a, b) *a += b
// #define atomic_fetch_sub(a, b) *a -= b

static inline double atomic_load_double(double *b)
{
    // printf("mouse load double: %f\n", *b);
    return *b;
}

static inline int atomic_load_int(atomic_int *b)
{
    // printf("mouse load int: %f\n", *b);
    return *b;
}

static inline void atomic_init_bool(atomic_bool *b, _Bool v)
{
    *b = v;
}

static inline void atomic_init_int(atomic_int *b, int v)
{
    *b = v;
}

static inline void atomic_store_double(double *b, double v)
{
    // printf("mouse store double %f to %f\n", v, *b);
    *b = v;
}

static inline void atomic_store_bool(atomic_bool *b, _Bool v)
{
    // printf("mouse store bool %d to %d\n", v, *b);
    *b = v;
}

static inline void atomic_store_int(atomic_int *b, int v)
{
    // printf("mouse store int %d to %d\n", v, *b);
    *b = v;
}

static inline void atomic_fetch_add_int(atomic_int *a, int b)
{
    // printf("mouse add %d to %d\n", b, *a);
    *a += b;
}
static inline void atomic_fetch_sub_int(atomic_int *a, int b)
{
    // printf("mouse sub %d to %d\n", b, *a);
    *a -= b;
}

#endif