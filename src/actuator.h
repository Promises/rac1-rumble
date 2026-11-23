/* SPDX-License-Identifier: GPL-3.0-only
 *
 * Going Commando's vibration mixer (actuator.c, copied): eight wave channels, each one of five
 * shapes on one of the pad's two motors, mixed once a tick into a value per motor. Ratchet & Clank
 * has the same mixer (actuator_CalcPower) but never runs it; Going Commando's adds permanent
 * channels, which play until they are stopped.
 *
 * The channel's layout and its fields' names are rac2-decomp's (WaveChannel,
 * src/levels/shared/L00_002D67A8-envelope.c); what each one does is read from
 * LevelActuatorCalcPower:
 *   kind       the shape: 0 an empty channel, 1 square, 2 triangle, 3 rising saw, 4 falling saw,
 *              5 cosine
 *   permanent  non-zero: the channel never counts its duration down, and plays until emptied
 *   output     the motor: 0 the small (on or off), 1 the large (0 to 255)
 *   multiply   non-zero: a modulator, which scales the motor's mix (its value / 255) rather than
 *              adds to it
 *   delay      ticks to wait before the wave starts (they count against duration)
 *   duration   ticks left: the channel plays while it counts down to 0, then empties
 *   frame      the wave's phase in ticks: one more each tick it plays
 *   rise       ticks of the first part of a period (the square's high, the rise)
 *   fall       ticks of the second part (the square's low, the fall)
 *   amplitude  the wave's height
 *   bias       added to the wave (its floor)
 */
#ifndef RAC1_RUMBLE_ACTUATOR_H
#define RAC1_RUMBLE_ACTUATOR_H

#ifdef __cplusplus
extern "C" {
#endif

/* rac2-decomp: src/levels/shared/L00_002D67A8-envelope.c */
typedef struct {
    unsigned char kind, permanent, output, multiply;
    short delay, duration, frame, rise, fall;
    unsigned char amplitude, bias;
} WaveChannel;

/* The number of channels (the game's g_wave_l00_channels[8]). */
#define WAVE_CHANNELS 8

/* One tick of the mixer over `g_wave_l00_channels` (WAVE_CHANNELS channels): `out[0]` and
 * `out[1]` get the small and the large motor's value (0 to 255), and it returns how many channels
 * are in use. The game runs it on its own global array; here the array is passed in. */
int LevelActuatorCalcPower(WaveChannel *g_wave_l00_channels, int *out);

#ifdef __cplusplus
}
#endif

#endif /* RAC1_RUMBLE_ACTUATOR_H */
