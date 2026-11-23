/* SPDX-License-Identifier: GPL-3.0-only
 *
 * Copied from rac2-decomp (OpenRAC/rac2-decomp, GPL-3.0), src/levels/shared/L00_002D67A8-envelope.c
 * at commit 4ea93ceaf43aec055e19211d36512e847860bac2, lines 16-117: LevelActuatorCalcPower, the
 * vibration mixer of Ratchet & Clank: Going Commando (USA v1.01, in the levels' shared code),
 * which follows Ratchet & Clank's actuator_CalcPower (rac1-decomp, adapted from Lombyte's
 * decompilation, GPL-3.0) and adds permanent channels.
 *
 * Unchanged but for the line marked (rac1-rumble): the channel array, the game's global there, is a
 * parameter of the same name here, and the game's math helpers are the C library's (below).
 * THIRD_PARTY_NOTICES.md has the details.
 */
#include <math.h>

#include "actuator.h"

/* (rac1-rumble) The game's helpers: IntToFloat converts, FloatToInt truncates toward 0,
 * math_fast_sin_or_cos_a is the game's table cosine here (Ratchet & Clank's FastCos in the same
 * place), the C library's cosf as exact or better. */
#define IntToFloat(x) ((float)(x))
#define FloatToInt(x) ((int)(x))
#define math_fast_sin_or_cos_a(x) cosf(x)

/* Mixes the eight active wave channels into two output values, one per output. Returns how many channels are in use. */
int LevelActuatorCalcPower(WaveChannel *g_wave_l00_channels, int *out) /* (rac1-rumble) the array a parameter */
{
    int scratch[3][4];
    int *outputCount;
    int *multiplyCount;
    int i, active = 0;

    multiplyCount = scratch[1];
    outputCount = scratch[2];
    for (i = 0; i < 2; i++) {
        out[i] = 0;
        scratch[0][i] = 0;
        scratch[1][i] = 0;
        scratch[2][i] = 0;
    }
    for (i = 0; i < 8; i++) {
        WaveChannel *channel;
        int phase, value;

        if (!g_wave_l00_channels[i].kind)
            continue;
        active++;
        if (!g_wave_l00_channels[i].permanent) {
            if (g_wave_l00_channels[i].duration-- <= 0)
                g_wave_l00_channels[i].kind = 0;
        }
        if (g_wave_l00_channels[i].delay) {
            g_wave_l00_channels[i].delay--;
            continue;
        }
        channel = &g_wave_l00_channels[i];
        g_wave_l00_channels[i].frame++;
        value = 0;
        phase = channel->frame % (channel->rise + channel->fall);
        switch (channel->kind) {
        case 0:
            break;
        case 1:
            if (phase < channel->rise)
                value = channel->amplitude + channel->bias;
            else
                value = channel->bias;
            break;
        case 2:
            if (phase < channel->rise)
                value = channel->amplitude * phase / channel->rise;
            else
                value = channel->amplitude * (channel->fall + channel->rise - phase) / channel->fall;
            value += channel->bias;
            break;
        case 3:
            if (phase < channel->rise)
                value = channel->amplitude * phase / channel->rise + channel->bias;
            break;
        case 4:
            if (phase >= channel->rise)
                value = channel->amplitude * (channel->fall + channel->rise - phase) / channel->fall + channel->bias;
            break;
        case 5: {
            float angle;

            if (phase < channel->fall)
                angle = IntToFloat(phase) * 3.1415927f / IntToFloat(channel->fall);
            else
                angle = 3.1415927f - IntToFloat(phase - channel->fall) * 3.1415927f / IntToFloat(channel->rise);
            angle = math_fast_sin_or_cos_a(angle);
            {
                float a = IntToFloat(channel->amplitude);
                value = FloatToInt((angle * a + a) * 0.5f) + channel->bias;
            }
            if (value > 255)
                value = 255;
            break;
        }
        }
        if (channel->multiply) {
            scratch[0][channel->output] += value;
            {
                int *p = multiplyCount + channel->output;
                *p = *p + 1;
            }
        } else {
            out[channel->output] += value;
            {
                int *p = outputCount + channel->output;
                *p = *p + 1;
            }
        }
    }
    for (i = 0; i < 2; i++) {
        int j = i;

        if (outputCount[j])
            out[j] /= outputCount[j];
        if (multiplyCount[j]) {
            scratch[0][j] /= multiplyCount[j];
            out[j] = out[j] * scratch[0][j] / 255;
        }
    }
    return active;
}
