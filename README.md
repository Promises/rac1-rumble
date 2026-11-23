# Rumble

A native mod for [OpenRAC](https://github.com/OpenRAC/OpenRAC)'s port of Ratchet & Clank:
**controller rumble in the style of Going Commando**. Ratchet & Clank has a vibration mixer, but the
retail game never drives the pad's motors; Going Commando does, for hits, hard landings, explosions
and its weapons. This mod plays rumble for the same kind of moments in Ratchet & Clank, on Going
Commando's own mixer, copied from its decompilation. PAL and NTSC-U.

## What rumbles

- **Hits taken**: a jolt that eases off over a third of a second, both motors; a heavier hit (the
  game's damage 2 or more) is stronger and longer. Dying: a long jolt, then nothing.
- **Hard landings**: after a long, fast fall (0.6 seconds or more coming down, at 12 units a second
  or faster), a thud on the large motor, stronger and longer the longer he fell, both motors from
  about a second down. Ordinary jumps, double jumps and Heli-Pack glides land quietly, and so does a
  fall that ends on a ledge or in water.
- **The wrench**: a short knock each time it connects with an enemy, a crate or a wall (swung or
  thrown).
- **Kills and crates**: a short bump for each enemy killed and each crate broken.
- **Weapons**, a kick per shot by weapon: light for the Blaster, Mine Glove, Decoy Glove, Drone
  Device, Taunter and Suck Cannon; heavier for the Bomb Glove, Visibomb Gun, Walloper and Glove of
  Doom; strongest for the Devastator and the R.Y.N.O. While the Pyrocitor, Tesla Claw or
  Morph-o-Ray fires, a steady low buzz of its own (a roar, a crackle, a hum).
- **Explosions and camera shake**: the game shakes its camera for explosions near it, quakes and
  scripted moments; the large motor follows the shake, as strong as it is, and the small one joins
  in for the strongest. Going Commando pairs its rumble with the camera's shake the same way.
- **Rails**: a low rattle while Ratchet grinds.

Nothing rumbles in the pause menu, a vendor, a dialog, a cutscene or a level load, nor while Ratchet
is dead; what was playing stops at once and does not come back after.

## Settings

In the launcher's Mods tab, read again whenever they change while the game runs:

- **Strength** (0 to 100%, default 100): the large motor's strength scales; the small motor is only
  on or off, and drops out below about 40%.
- **Hits taken**, **Hard landings**, **Wrench and kills**, **Weapons**, **Explosions and camera
  shake**, **Rails**: each on by default.

The player's own rumble setting in OpenRAC applies on top.

## Needs

**OpenRAC with mod interface 1.37**, for `set_motors`, the call that drives the pads' motors:
OpenRAC pull request #803 (interface 1.36) and the `set_motors` pull request stacked on it (1.37),
until a release has them. A port without it refuses the mod and says why.

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
| Hard landings | the hero's time off the ground and his speed downward, each tick |
| The wrench | `rac1.moby.wrench.update`: the wrench's hit latch (`Hero.unkA64`) set during its update |
| Kills and crates | `rac1.moby.record_death` |
| Weapons | `rac1.weapon.take_ammo` (a shot taken), by the weapon in hand |
| Camera shake | the level camera's two shake channels, each tick: amplitude times (frames left / frames)², as the game's `apply_camera_shake` scales its shake |
| Rails | the hero's state (grinding), each tick |
| Quiet | `play_status`: only in play, not paused, in a menu or a scene |

After each tick the motors get the mix (`set_motors`, every tick while it plays, as a command holds
a fifth of a second; then 0, 0 once).

The game has no hook point at its explosion spawn; explosions reach the rumble through the camera's
shake, so one out of view (which shakes nothing) does not rumble. A hook point there would be an
OpenRAC addition.

## Build

```sh
python3 tools/fetch_sdk.py          # the SDK sdk.lock pins, into .sdk/
cmake -S . -B build -G Ninja
cmake --build build && ctest --test-dir build
python3 .sdk/openrac-mod-sdk-*/tools/package_mod.py build/package/rumble
```

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
