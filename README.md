# Rumble

![Rumble](docs/banner.png)

A native mod for [OpenRAC](https://github.com/OpenRAC/OpenRAC)'s port of Ratchet & Clank:
**controller rumble in the style of Going Commando**. Ratchet & Clank has a vibration mixer, but the
retail game never drives the pad's motors; Going Commando does, for hits, jumps, the wrench slam,
its weapons and its vehicles. This mod plays rumble for the same kind of moments in Ratchet & Clank, on Going
Commando's own mixer, copied from its decompilation. PAL and NTSC-U.

## What rumbles

- **Hits taken**: a jolt that eases off over a third of a second, both motors; a heavier hit (the
  game's damage 2 or more) is stronger and longer. Dying: a long jolt, then nothing.
- **Jumps**, where Going Commando rumbles for its own: a low push on both motors while the
  Thruster-Pack's long jump lasts (and the jump that shares it in Going Commando, state 0xD),
  weaker than a hit and longer, ending as the jump's state does; a flick of the small motor for a
  wall jump. Ordinary jumps, high jumps and double jumps stay quiet, as in Going Commando.
- **The wrench**: a short knock each time it connects with an enemy, a crate or a wall (swung or
  thrown), and a thump as the wrench slam strikes the ground.
- **Kills and crates**: a short bump for each enemy killed and each crate broken.
- **Weapons**, a kick per shot by weapon: light for the Blaster, Mine Glove, Decoy Glove, Drone
  Device, Taunter and Suck Cannon; heavier for the Bomb Glove, Visibomb Gun, Walloper and Glove of
  Doom; strongest for the Devastator and the R.Y.N.O. While the Pyrocitor, Tesla Claw or
  Morph-o-Ray fires, a steady low buzz of its own (a roar, a crackle, a hum), which stops the tick
  its button is let go, as Going Commando's looping weapon waves do.
- **Explosions and camera shake**: the game shakes its camera for explosions near it, quakes and
  scripted moments; the large motor follows the shake, as strong as it is, and the small one joins
  in for the strongest. Going Commando pairs its rumble with the camera's shake the same way.
- **Rails and swimming**: a low rattle while Ratchet grinds, and a steady push while the Hydro-Pack
  dashes him under water (Going Commando rumbles through its own dash).
- **Hover hum** (off by default): a faint, steady hum while the Thruster-Pack's hover or the
  Heli-Pack's glide holds him up. This one is not Going Commando's, which has none. With the
  Thruster Charge mod, its charge runs in the hover's state, so it hums too.

Nothing rumbles in the pause menu, a vendor, a dialog, a cutscene or a level load, nor while Ratchet
is dead; what was playing stops at once and does not come back after.

## Settings

In the launcher's Mods tab, read again whenever they change while the game runs:

- **Strength** (0 to 100%, default 100): the large motor's strength scales; the small motor is only
  on or off, and comes on less often (at 50%, every other tick of a kick).
- **Hits taken**, **Jumps**, **Wrench and kills**, **Weapons**, **Explosions and
  camera shake**, **Rails and swimming**: each on by default.
- **Hover hum**: off by default.

The player's vibration settings in OpenRAC's Controls menu apply; with the mod loaded they are
available in Ratchet & Clank too (OpenRAC shows them for a game that never rumbles once a mod has
driven the motors, so the mod sends a 0, 0 at load, or on its first tick when the call at load comes
too early).

## Needs

**OpenRAC with mod interface 1.37** ([OpenRAC #804](https://github.com/OpenRAC/OpenRAC/pull/804)),
for `set_motors`, the call that drives the pads' motors. A port without it refuses the mod and says
why.

The `dualsense` mod has a vibration of its own (hits, crates, pickups) that it plays on a DualSense
through SDL itself, past `set_motors`. The two load together (no conflict is declared), but on a
DualSense both then write the same motors and the last write wins: turn off its **Vibration**
setting, or this mod, there.

## How it works

Going Commando's mixer, `LevelActuatorCalcPower`, mixes eight wave channels (square, triangle,
rising and falling saw, cosine; each on the small or the large motor, with a delay, a duration, an
amplitude and a bias) into one value per motor each tick. A channel counts its duration down and
empties, unless it is permanent: then it plays until it is stopped. Ratchet & Clank has the same
mixer without permanent channels (`actuator_CalcPower`), and nothing calls it; Going Commando drives
its mixer with a pattern player whose patterns are data on its disc. The mod copies Going
Commando's mixer (`src/actuator.c`, unchanged but for its channel array being a parameter) and
plays its own patterns on it (`src/rumble.hpp`), in two banks: one for short events, and one for
what lasts, a permanent wave started as a stream weapon starts firing or Ratchet lands on a rail,
and stopped as it stops. The mixer averages the waves on a motor, so a buzz in the same bank as a
hit would halve it; the two banks and the camera's shake are joined by the stronger instead.

Where the moments come from:

| Moment | From |
|---|---|
| Hits taken | `rac1.player.take_damage`, its damage |
| Dying | the hero's state (dying, drowning, a fatal fall...) or no health, each tick |
| Jumps | `rac1.player.enter_move_state`: states 0x10 (the Thruster-Pack's long jump), 0xD and 0x11 (the wall jump) taken; the next state taken ends the push |
| The wrench | `rac1.moby.wrench.update`: the wrench's hit latch (`Hero.unkA64`) set during its update |
| The wrench slam | the hero in state 0x14 with his animation passing frame 27 this tick, where `wrench_update` strikes the ground |
| Kills and crates | `rac1.moby.record_death` |
| Weapons | `rac1.weapon.take_ammo` (a shot taken), by the weapon in hand; a stream weapon's buzz stops when its button is let go (the pad's held buttons and the item in hand's button mask, `items[0].button_mask`, as the stream weapons' updates read them) or its use is locked |
| Camera shake | the level camera's two shake channels, each tick: amplitude times (frames left / frames)², as the game's `apply_camera_shake` scales its shake |
| Rails, the Hydro-Pack's dash, the hover | the hero's state (grinding; 0x35; 0x81 or 0x8), each tick |
| Quiet | `play_status`: only in play, not paused, in a menu or a scene |

After each tick the motors get the mix (`set_motors`, every tick while it plays, as a command holds
a fifth of a second; then 0, 0 once). The small motor is only on or off: as in Going Commando's
actuator update, its mixed value is how often it is on (a running sum gains the value each tick,
and the motor is on for each 255 it passes), so a wave of 64 turns it on one tick in four.

### Going Commando's rumble, and what is here

Every call of Going Commando's pattern player (rac2-decomp's `actuator_play_pattern`,
`actuator_play_waves`, `actuator_stop_all` and `actuator_end_looping_waves`), found in all 27 level
programs of its USA v2.00 disc. Its patterns and waves are data on its disc, so nothing of them is
copied: the waves here are this mod's own, set by hand, in the order Going Commando's come in
(which is longer or stronger, which motor, looping or not).

| Going Commando | Ratchet & Clank | Here |
|---|---|---|
| A hit taken (entering hero states 0x16 and 0x8C): short and strong, the large motor | `take_damage` | Hits taken (and a heavier hit, the mod's own) |
| The long jump and state 0xD (entering 0x10, 0xD): a low push on both motors, longer than a hit and weaker, until the next change of state | the same states | Jumps |
| The wall jump (entering 0x11): the lightest, a flick of the small motor | 0x11 | Jumps |
| The wrench slam striking the ground (0x14): both motors, the large one below a hit's, with a camera shake | 0x14, its animation's frame 27 | Wrench and kills |
| The swim dash (0x34): every tick it lasts, ended by the change of state | 0x35, the Hydro-Pack's dash | Rails and swimming |
| State 0x1F with an object in hand | none found (Ratchet & Clank's 0x1F is an idle wait) | not here |
| Weapons, a short kick per shot, on both motors for those checked (`minirocket_tube_update`, `blitz_gun_update`, `lancer_update`, `gravity_bomb_launcher_update`, `mini_l00_nuke_update`, `vaporizer_update`, `bouncer_update`, `megarocket_cannon_update`, `UpdateLockOnWeapon`, `UpdateSpreadWeapon`, `UpdateScopedWeapon`), a rising wave as `charging_weapon_update` charges | Ratchet & Clank's weapons, a shot each `take_ammo` | Weapons (kicks by weapon, the mod's own strengths) |
| `lancer_update` for its second moby class (the upgraded Lancer, it seems): a looping wave from its first shot until its button is let go or its ammo runs out (`actuator_end_looping_waves`) | the stream weapons: Pyrocitor, Tesla Claw, Morph-o-Ray | Weapons: a buzz that stops as the button is let go |
| Every change of the hero's state stops all waves (`actuator_stop_all`), but for 50 ticks after the wrench slam strikes | | the jumps' push only; the mod's other events play their length |
| Dying: nothing (the change to the dying state stops everything) | | Hits taken: a long jolt, the mod's own |
| Hoverbike races: a rocket fired (`racer_update_item_use`), a crash (`hoverboard_update_rider_physics`) | the Hoverboard races: no hook point at a crash | camera shake only, if any |
| Space combat (Feltzin and the orbits): the ship's weapons, its collisions, boss parts and shots | none: Ratchet & Clank has no space combat | not here |
| Oozla: the ferry (looping while it carries Ratchet) and the grabber | none | not here |
| The small motor: on as often as its mixed value says (a running sum) | | the same since 1.0.1 |

Not Going Commando's (none of its code rumbles for them): kills and crates, the wrench's knock,
rails, the camera's shake followed as a level, the hover hum, the death jolt. Landings stay quiet,
as in Going Commando.

The game has no hook point at its explosion spawn; explosions reach the rumble through the camera's
shake, so one out of view (which shakes nothing) does not rumble. A hook point there would be an
OpenRAC addition.

## Next version

Going Commando also rumbles in its vehicles: the hoverbike races (a rocket fired, a crash with its
explosion) and space combat (the ship's weapons, its hits and collisions, boss parts). Ratchet &
Clank's counterparts, the Hoverboard races and the ship fights (the fight with Quark's ship among
them), need hook points or state reads this mod does not have yet; until then they rumble only
through the camera's shake.

## Build

```sh
python3 tools/fetch_sdk.py          # the SDK sdk.lock pins, into .sdk/
cmake -S . -B build -G Ninja
cmake --build build && ctest --test-dir build
python3 .sdk/openrac-mod-sdk-*/tools/package_mod.py build/package/rumble
```

Until an `sdk-v*` release has interface 1.37, `sdk.lock` pins the SDK's source at a commit of
OpenRAC's develop (`git` and `commit`; `tools/fetch_sdk.py` fetches that commit's `sdk/` alone);
it goes back to a release's `url` and `sha256` once one does.

## Credits

The mixer is Going Commando's, from OpenRAC's [rac2-decomp](https://github.com/OpenRAC/rac2-decomp)
(GPL-3.0); it follows Ratchet & Clank's own, which OpenRAC's
[rac1-decomp](https://github.com/OpenRAC/rac1-decomp) (GPL-3.0) adapted from Lombyte's NTSC-U
decompilation (GPL-3.0). The game's memory layout and hero states are rac1-decomp's.

## License

GNU General Public License version 3 ([LICENSE](LICENSE)). The files copied from rac2-decomp are
GPL-3.0-only, as that project gives no later version; the mod's own files are GPL-3.0-or-later.
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) names the source, its commit and the lines copied;
the package carries it as `LICENSE.THIRD_PARTY_NOTICES.md`.
