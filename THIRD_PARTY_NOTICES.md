# Third-party notices

Rumble is licensed under the GNU General Public License version 3 ([LICENSE](LICENSE)). The mod's
own files are GPL-3.0-or-later; the files copied from Going Commando's decompilation are under that
project's license, the GPL version 3 (it names no later version, so they are GPL-3.0-only). Each
file says which in its `SPDX-License-Identifier` line.

## Going Commando's decompilation: OpenRAC/rac2-decomp

- Source: <https://github.com/OpenRAC/rac2-decomp>, commit
  `4ea93ceaf43aec055e19211d36512e847860bac2` (2026-11-17), as the submodule `games/rac2/decomp`
  of OpenRAC's `develop`.
- License: GNU General Public License version 3 (its `LICENSE`: Copyright (c) 2026 Kryštof
  "Lynder063" Malinda). The project was MIT up to its commit `d46a3539` (2026-10-11); the file
  copied here was added after that (2026-10-29, `58ccd57c`), so it carries no MIT notice.
- Target: Ratchet & Clank: Going Commando, USA v1.01.

Copied, unchanged but for the lines marked `(rac1-rumble)`:

| File here | What | rac2-decomp file and lines |
|---|---|---|
| `src/actuator.c` | `LevelActuatorCalcPower`, the vibration mixer | `src/levels/shared/L00_002D67A8-envelope.c`: 16-117 |
| `src/actuator.h` | `WaveChannel`, the mixer's channel | `src/levels/shared/L00_002D67A8-envelope.c`: 9-13 |

The changes: the channel array, a global of the game's (`g_wave_l00_channels[8]`), is a parameter
of the same name, so the mod runs two banks of it; and the game's helpers it calls (`IntToFloat`,
`FloatToInt`, `math_fast_sin_or_cos_a`) are a cast, a truncation and the C library's `cosf` (the
cosine Ratchet & Clank's copy of the mixer calls in the same place, `FastCos`).

rac2-decomp's mixer follows Ratchet & Clank's `actuator_CalcPower` (rac1-decomp,
`src/game/gameplay/actuator.c`, which rac1-decomp adapted from Lombyte's decompilation of the NTSC-U
game, GPL-3.0, <https://github.com/lombyte-project/Lombyte>), and adds permanent channels. Going
Commando's pattern player and its patterns are data on the game's disc; none are taken: the
patterns here are this mod's own.

## Ratchet & Clank's decompilation: OpenRAC/rac1-decomp

- Source: <https://github.com/OpenRAC/rac1-decomp>, commit
  `f911d6e4b9711071f0daaad95b4a04c2c8aa3c61` (2026-11-08), as the submodule `games/rac1/decomp`
  of OpenRAC (GNU GPL version 3).
- The mod reads the game's memory at offsets taken from its field layout
  (`include/structs/hero.h`), its hero states (`include/enums/hero_state.h`, enumerations from
  Lombyte's project) and what `wrench_update`, `use_ammo`, `hero_change_health` and
  `apply_camera_shake` do with them, and the level camera's address from OpenRAC's port
  (`port/game/rac1/host/camera_override.c`, `port/game/rac1-ntsc/host/rac1_version.h`, GPL-3.0).
  No code is copied from it.

## The OpenRAC mod SDK

The headers under `.sdk/` (fetched, not in this repository) are MIT, Copyright (c) 2026 the
OpenRAC contributors; the mod includes them.

Ratchet & Clank and Ratchet & Clank: Going Commando are trademarks of Sony Interactive
Entertainment, developed by Insomniac Games. This mod is not affiliated with or endorsed by
either, and contains none of their code or assets: the decompiled code it copies is the
decompilation project's reconstruction, and the game runs from the player's own disc.
