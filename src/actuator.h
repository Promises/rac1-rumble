/* SPDX-License-Identifier: GPL-3.0-only
 *
 * Ratchet & Clank's vibration mixer (actuator.c, copied): eight wave slots, each one of five
 * shapes on one of the pad's two motors, mixed once a tick into a value per motor. The retail
 * game has the mixer but never runs it (its call to the pad's motors is dead-stripped).
 *
 * The slot's layout is rac1-decomp's (include/structs/data/game_gameplay_actuator.h, L00Entry), its
 * fields' names are rac1-decomp's too; what each one does is read from actuator_CalcPower:
 *   f00  the shape: 0 an empty slot, 1 square, 2 triangle, 3 rising saw, 4 falling saw, 5 cosine
 *   f02  the motor: 0 the small (on or off), 1 the large (0 to 255)
 *   f03  non-zero: a modulator, which scales the motor's mix (its value / 255) rather than adds
 *   f04  ticks to wait before the wave starts (they count against f06)
 *   f06  ticks left: the slot plays while it counts down to 0, then empties
 *   f08  the wave's phase in ticks: one more each tick it plays
 *   f0a  ticks of the first part of a period (the square's high, the rise)
 *   f0c  ticks of the second part (the square's low, the fall)
 *   f0e  the wave's height
 *   f0f  added to the wave (its floor)
 */
#ifndef RAC1_RUMBLE_ACTUATOR_H
#define RAC1_RUMBLE_ACTUATOR_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int16_t s16;
typedef uint16_t u16;
typedef uint8_t u8;
typedef int32_t s32;
typedef float f32;

/* rac1-decomp: include/structs/data/game_gameplay_actuator.h */
typedef struct {
    s16 f00;
    u8 f02;
    u8 f03;
    s16 f04;
    u16 f06;
    s16 f08;
    s16 f0a;
    s16 f0c;
    u8 f0e;
    u8 f0f;
} L00Entry;

/* The number of slots (the game's actuator_wave[8]). */
#define ACTUATOR_SLOTS 8

/* One tick of the mixer over `actuator_wave` (ACTUATOR_SLOTS slots): `p1[0]` and `p1[1]` get the
 * small and the large motor's value (0 to 255), and it returns how many slots were playing. The game
 * runs it on its own global array; here the array is passed in. */
s32 actuator_CalcPower(L00Entry *actuator_wave, s32 *p1);

#ifdef __cplusplus
}
#endif

#endif /* RAC1_RUMBLE_ACTUATOR_H */
