/* SPDX-License-Identifier: GPL-3.0-only
 *
 * Copied from rac1-decomp (OpenRAC/rac1-decomp, GPL-3.0), src/game/gameplay/actuator.c at commit
 * f911d6e4b9711071f0daaad95b4a04c2c8aa3c61, lines 19-128: actuator_CalcPower, the vibration
 * mixer of Ratchet & Clank (PAL text 0x1E9080-0x1E94E8), which rac1-decomp adapted from Lombyte's
 * NTSC decompilation (GPL-3.0): src/overlays/shared/start_001eaa08.c, FUN_L00_001eaa08.
 *
 * Unchanged but for the lines marked (rac1-rumble): the wave array, the game's global there, is a
 * parameter of the same name here, and the game's math helpers are the C library's (below).
 * THIRD_PARTY_NOTICES.md has the details.
 */
#include <math.h>

#include "actuator.h"

/* (rac1-rumble) The game's helpers: IntToFloat converts, truncate_float_to_s32 truncates toward 0,
 * FastCos is the game's table cosine (here the C library's, as exact or better). */
#define IntToFloat(x) ((f32)(x))
#define truncate_float_to_s32(x) ((s32)(x))
#define FastCos(x) cosf(x)

/* Adapted from Lombyte's project (GPL-3.0) for PAL by OpenRAC's tools/port.py: src/overlays/shared/start_001eaa08.c, FUN_L00_001eaa08. */
s32 actuator_CalcPower(L00Entry *actuator_wave, s32 *p1) { /* (rac1-rumble) the array a parameter */
    s32 aa[3][4];
    s32 *q2;
    s32 *q1;
    s32 i;
    s32 count;

    count = 0;
    q1 = aa[1];
    q2 = aa[2];
    for (i = 0; i < 2; i++) {
        p1[i] = 0;
        aa[0][i] = 0;
        aa[1][i] = 0;
        aa[2][i] = 0;
    }
    for (i = 0; i < 8; i++) {
        L00Entry *e;
        s32 rem;
        s32 res;
        u16 v;

        if (actuator_wave[i].f00 == 0) {
            continue;
        }
        e = &actuator_wave[i];
        count++;
        v = actuator_wave[i].f06;
        actuator_wave[i].f06 = v - 1;
        if ((s16)v <= 0) {
            e->f00 = 0;
        }
        if (actuator_wave[i].f04 != 0) {
            actuator_wave[i].f04 = actuator_wave[i].f04 - 1;
            continue;
        }
        res = 0;
        actuator_wave[i].f08++;
        rem = e->f08 % (e->f0a + e->f0c);
        switch (e->f00) {
            case 0:
                break;
            case 1:
                if (rem < e->f0a) {
                    res = e->f0e + e->f0f;
                } else {
                    res = e->f0f;
                }
                break;
            case 2:
                if (rem < e->f0a) {
                    res = (e->f0e * rem) / e->f0a;
                } else {
                    res = (e->f0e * (e->f0c + e->f0a - rem)) / e->f0c;
                }
                res = res + e->f0f;
                break;
            case 3:
                if (rem < e->f0a) {
                    res = (e->f0e * rem) / e->f0a + e->f0f;
                }
                break;
            case 4:
                if (rem >= e->f0a) {
                    res = (e->f0e * (e->f0c + e->f0a - rem)) / e->f0c + e->f0f;
                }
                break;
            case 5: {
                f32 f;
                f32 g;
                if (rem < e->f0c) {
                    f = IntToFloat(rem) * 3.1415927f / IntToFloat(e->f0c);
                } else {
                    f = 3.1415927f - IntToFloat(rem - e->f0c) * 3.1415927f / IntToFloat(e->f0a);
                }
                f = FastCos(f);
                g = IntToFloat(e->f0e);
                res = truncate_float_to_s32((f * g + g) * 0.5f) + e->f0f;
                if (res >= 256) {
                    res = 255;
                }
                break;
            }
        }
        if (e->f03 != 0) {
            aa[0][e->f02] = aa[0][e->f02] + res;
            {
                s32 *t = q1 + e->f02;
                *t = *t + 1;
            }
        } else {
            p1[e->f02] = p1[e->f02] + res;
            {
                s32 *t = q2 + e->f02;
                *t = *t + 1;
            }
        }
    }
    for (i = 0; i < 2; i++) {
        if (q2[i] != 0) {
            p1[i] = p1[i] / q2[i];
        }
        if (q1[i] != 0) {
            aa[0][i] = aa[0][i] / q1[i];
            p1[i] = (p1[i] * aa[0][i]) / 255;
        }
    }
    return count;
}
