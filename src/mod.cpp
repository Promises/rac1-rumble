// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Henning Berge
//
// Rumble: Going Commando's kind of controller rumble for Ratchet & Clank, which never drives the
// pad's motors (rumble.hpp says what rumbles and how).
//
// Hook points:
//   rac1.player.take_damage   a hit taken, by its damage
//   rac1.weapon.take_ammo     a shot: the ammo taken, by the weapon in hand
//   rac1.moby.wrench.update   the wrench's hit latch set during its update: it connected
//   rac1.moby.record_death    an enemy killed, a crate broken
//   rac1.player.enter_move_state  a state taken: the Thruster-Pack's long jump, the wall jump
// and after each tick (on_frame): the game's phase (play_status: rumble only in play, not paused, in
// a menu or a scene), the hero (dying, landing after a long fall, the wrench slam striking, on a
// rail, the Hydro-Pack's dash, a pack's hover) and the camera's shake (explosions, quakes, anything
// that shakes it), then the motors (set_motors, interface 1.37).
// Each hook point is optional: a disc without one plays the rest.
#include <cstdint>
#include <string>

#include "openrac/games/rac1.hpp"
#include "openrac/mod.hpp"

#include "game.hpp"
#include "layout.hpp"
#include "rumble.hpp"

namespace {

namespace rr = rac1_rumble;

struct State {
    openrac::Mod* mod = nullptr;
    rr::Disc disc{};
    rumble::Rumble rumble;
};

State& state() {
    static State s;
    return s;
}

rumble::Settings read_settings(const openrac::Mod& mod) {
    rumble::Settings s;
    s.strength = mod.setting<int>("strength");
    s.damage = mod.setting<bool>("damage");
    s.landings = mod.setting<bool>("landings");
    s.melee = mod.setting<bool>("melee");
    s.weapons = mod.setting<bool>("weapons");
    s.explosions = mod.setting<bool>("explosions");
    s.grind = mod.setting<bool>("grind");
    s.jumps = mod.setting<bool>("jumps");
    s.hover = mod.setting<bool>("hover");
    return s;
}

void tick() {
    State& s = state();
    openrac::Mod& mod = *s.mod;
    const int level = mod.loaded_level();
    const bool playing = level >= 0 && mod.play_status().phase == OPENRAC_PLAY_PLAYING;
    const std::uint32_t shake = playing ? mod.relocate_level(0, s.disc.shake_l00) : 0;
    const rumble::Motors out = s.rumble.tick(rr::read_frame(mod.memory(), s.disc, playing, shake));
    if (out.send) {
        mod.set_motors(0, out.small, out.large);
    }
}

// Takes hook point H for `callable`, or says in the log what goes without it.
template <class H, class F>
void hook(openrac::Mod& mod, F&& callable, const char* without) {
    try {
        mod.hook<H>(std::forward<F>(callable));
    } catch (const openrac::Error& e) {
        mod.warn(std::string(e.what()) + ": no rumble for " + without);
    }
}

void load(openrac::Mod& mod) {
    if (mod.game() != "rac1") {
        throw openrac::Error("this mod is for Ratchet & Clank");
    }
    const bool ntsc = mod.version() == "ntsc";
    if (!ntsc && mod.version() != "pal") {
        throw openrac::Error("this mod knows the PAL and NTSC-U discs only");
    }
    if (!mod.has_set_motors()) {
        throw openrac::Error("needs OpenRAC with mod interface 1.37 (set_motors)");
    }
    State& s = state();
    s.mod = &mod;
    s.disc = rr::disc_for(ntsc);
    s.rumble.set_tick(s.disc.tick_seconds);
    namespace rac1 = openrac::rac1;

    hook<rac1::Hero_TakeDamage>(
        mod,
        [](auto original, std::int32_t amount) {
            original(amount);
            state().rumble.hurt(amount);
        },
        "hits taken");

    hook<rac1::Weapon_TakeAmmo>(
        mod,
        [](auto original, std::int32_t item, std::int32_t amount) -> std::int32_t {
            const std::int32_t taken = original(item, amount);
            if (taken != 0) {
                const std::int32_t weapon =
                    item >= 0 ? item : state().mod->memory().read<std::int32_t>(state().disc.hero + rr::kItemInHand);
                state().rumble.weapon_fired(weapon);
            }
            return taken;
        },
        "weapons");

    hook<rac1::Moby_Wrench_Update>(
        mod,
        [](auto original, std::uint32_t wrench) {
            const openrac::Memory m = state().mod->memory();
            const std::uint32_t latch = state().disc.hero + rr::kWrenchHit;
            const std::int32_t before = m.read<std::int32_t>(latch);
            original(wrench);
            if (before == 0 && m.read<std::int32_t>(latch) != 0) {
                state().rumble.wrench_hit();
            }
        },
        "wrench hits");

    hook<rac1::Moby_RecordDeath>(
        mod,
        [](auto original, std::uint32_t moby, std::int32_t flags, std::int32_t index) {
            original(moby, flags, index);
            state().rumble.kill();
        },
        "kills and crates");

    // The state as taken: a jump refused (its lock) puts the last state back and answers 0.
    hook<rac1::Hero_EnterMoveState>(
        mod,
        [](auto original, std::int32_t asked, std::int32_t force) -> std::int32_t {
            const std::int32_t took = original(asked, force);
            if (took != 0 &&
                state().mod->memory().read<std::int32_t>(state().disc.hero + rr::kHeroState) == asked) {
                state().rumble.jumped(asked);
            }
            return took;
        },
        "jumps");

    mod.on_frame([] { tick(); });

    s.rumble.configure(read_settings(mod));
    if (mod.has_settings_set()) {
        mod.on_settings([] { state().rumble.configure(read_settings(*state().mod)); });
    }
    mod.info("controller rumble at " + std::to_string(s.rumble.settings().strength) + "%");
}

}  // namespace

OPENRAC_MOD_CPP("rumble", "1.0.0", load)
