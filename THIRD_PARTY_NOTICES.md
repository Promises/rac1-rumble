# Third-party notices

Rumble is licensed under the GNU General Public License version 3 ([LICENSE](LICENSE)). The mod's
own files are GPL-3.0-or-later; the file copied from Ratchet & Clank's decompilation is under that
project's license, the GPL version 3 (it names no later version, so it is GPL-3.0-only). Each
file says which in its `SPDX-License-Identifier` line.

## Ratchet & Clank's decompilation: OpenRAC/rac1-decomp

- Source: <https://github.com/OpenRAC/rac1-decomp>, commit
  `f911d6e4b9711071f0daaad95b4a04c2c8aa3c61` (2026-11-08), as the submodule `games/rac1/decomp`
  of OpenRAC.
- License: GNU General Public License version 3 (its `LICENSE` and `README.md`).
- Target: Ratchet & Clank, PAL.

Copied, unchanged but for the lines marked `(rac1-rumble)`:

| File here | Function | rac1-decomp file and lines | PAL |
|---|---|---|---|
| `src/actuator.c` | `actuator_CalcPower`, the vibration mixer | `src/game/gameplay/actuator.c`: 19-128 | text `0x1E9080`-`0x1E94E8` |
| `src/actuator.h` | `L00Entry`, the mixer's slot | `include/structs/data/game_gameplay_actuator.h`: 6-17 | |

The changes: the wave array, a global of the game's (`actuator_wave[8]`), is a parameter of the same
name, so the mod runs two banks of it; and the game's helpers it calls (`IntToFloat`,
`truncate_float_to_s32`, `FastCos`) are a cast, a truncation and the C library's `cosf`.

rac1-decomp adapted this function from Lombyte's decompilation of the NTSC-U game (GPL-3.0,
<https://github.com/lombyte-project/Lombyte>): `src/overlays/shared/start_001eaa08.c`,
`FUN_L00_001eaa08`, carried over to PAL by OpenRAC's `tools/port.py`.

The mod's own code reads the game's memory at offsets taken from rac1-decomp's field layout
(`include/structs/hero.h`), its hero states (`include/enums/hero_state.h`, enumerations from
Lombyte's project) and what `wrench_update`, `use_ammo`, `hero_change_health` and
`apply_camera_shake` do with them, and the level camera's address from OpenRAC's port
(`port/game/rac1/host/camera_override.c`, `port/game/rac1-ntsc/host/rac1_version.h`, GPL-3.0); no
other code is copied.

Going Commando runs the same mixer (rac2-decomp, `src/levels/shared/L00_002D67A8-envelope.c`,
`LevelActuatorCalcPower`) with its own pattern player; its patterns are data on the game's disc, so
none are taken: the patterns here are this mod's own.

## The OpenRAC mod SDK

The headers under `.sdk/` (fetched, not in this repository) are MIT, Copyright (c) 2026 the
OpenRAC contributors; the mod includes them.

Ratchet & Clank and Ratchet & Clank: Going Commando are trademarks of Sony Interactive
Entertainment, developed by Insomniac Games. This mod is not affiliated with or endorsed by
either, and contains none of their code or assets: the decompiled code it copies is the
decompilation projects' reconstruction, and the game runs from the player's own disc.
