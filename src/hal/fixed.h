/*
 * fixed.h - 8.8 fixed-point arithmetic
 *
 * The base X68000 (68000 @ 10MHz) has no FPU, and its MULU/MULS instructions
 * are only 16x16 -> 32. A 16.16 multiply would compile to a 32x32 library call
 * (very slow). So the shared game logic uses 8.8 fixed-point by default, whose
 * multiply fits a single 16x16 MULU when operands are small.
 *
 * float is BANNED in shared logic (src/game, src/hal video, src/vhw math) so
 * that the PC and X68000 builds produce bit-identical simulation results. This
 * is what makes input-replay parity testing possible.
 */
#ifndef HAL_FIXED_H
#define HAL_FIXED_H

#include <stdint.h>

typedef int32_t fix8;              /* 8.8 fixed-point, range approx +-8388608 */

#define FIX8_SHIFT        8
#define FIX8_ONE          (1 << FIX8_SHIFT)

#define FIX8(n)           ((fix8)((n) * FIX8_ONE))          /* literal -> fix8 */
#define FIX8_FROM_INT(n)  ((fix8)((int32_t)(n) << FIX8_SHIFT))
#define FIX8_TO_INT(f)    ((int)((f) >> FIX8_SHIFT))        /* floor */
#define FIX8_FRAC(f)      ((f) & (FIX8_ONE - 1))
#define FIX8_MUL(a, b)    ((fix8)(((int32_t)(a) * (int32_t)(b)) >> FIX8_SHIFT))
#define FIX8_DIV(a, b)    ((fix8)(((int32_t)(a) << FIX8_SHIFT) / (int32_t)(b)))

#endif /* HAL_FIXED_H */
