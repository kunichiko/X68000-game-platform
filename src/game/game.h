/*
 * game.h - shared game logic. Portable C, 8.8 fixed-point, no float, no
 * platform calls other than the HAL. Compiled identically for PC/Mac and
 * X68000.
 */
#ifndef GAME_H
#define GAME_H

#include <stdint.h>

void game_init(void);
void game_step(uint16_t pad);   /* advance one fixed 60Hz simulation tick */

#endif /* GAME_H */
