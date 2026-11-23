// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Henning Berge
//
// The rumble: what the game does turned into the pad's two motors, a tick at a time, in the style of
// Going Commando (which drives them; Ratchet & Clank never does). Nothing here touches the game: the
// hooks report events (a hit taken, a shot, a wrench hit, a kill), each tick reports a Frame (what
// the game is doing, game.hpp reads it), and tick() answers what to send.
//
// The waves play on Ratchet & Clank's own mixer, actuator_CalcPower (actuator.c, copied), in two
// banks of its eight slots: one for the short events, one for what lasts (a stream weapon held, a
// rail ground). The mixer averages the waves on a motor; the two banks and the camera's shake are
// put together by the stronger, so a held flamethrower's low buzz never halves a hit.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

#include "actuator.h"

namespace rumble {

// The mixer's motors (a wave's f02) and shapes (f00).
constexpr std::uint8_t kSmall = 0;
constexpr std::uint8_t kLarge = 1;
enum Shape : std::int16_t { kSquare = 1, kTriangle = 2, kSawUp = 3, kSawDown = 4, kCosine = 5 };

// The small motor is on or off: on while its mixed value, times the strength, is at least this.
constexpr int kSmallOn = 96;

// One wave of a pattern, in seconds (made ticks as it starts, at the game's tick).
//   kind  kPulse: flat at `height` for `length`
//         kFade: a falling saw from `height` to nothing over `length`
//         kEase: a half cosine from `height` to nothing over `length` (softer at both ends)
//         kWobble: a triangle between `floor` and `floor + height`, `period` a cycle, for `length`
struct Wave {
    enum Kind : std::uint8_t { kPulse, kFade, kEase, kWobble } kind;
    std::uint8_t motor;
    std::uint8_t height;
    float length;
    float delay = 0.0f;
    float period = 0.0f;
    std::uint8_t floor = 0;
};

// Up to three waves started together.
struct Pattern {
    std::array<Wave, 3> waves;
    int count;
};

// --- The patterns (seconds; heights 0 to 255) ---

namespace patterns {

// A hit taken: the large motor eased down from a jolt, the small one on for its first moment.
constexpr Pattern kHurt{{{{Wave::kEase, kLarge, 200, 0.35f}, {Wave::kPulse, kSmall, 255, 0.12f}}}, 2};
// A heavier hit (the game's damage 2 or more).
constexpr Pattern kHurtHeavy{{{{Wave::kEase, kLarge, 255, 0.50f}, {Wave::kPulse, kSmall, 255, 0.20f}}}, 2};
// Dying: a held jolt, then a long ease out.
constexpr Pattern kDeath{{{{Wave::kPulse, kLarge, 255, 0.25f},
                           {Wave::kEase, kLarge, 255, 0.60f, 0.25f},
                           {Wave::kPulse, kSmall, 255, 0.30f}}},
                         3};
// The wrench connecting: a short knock.
constexpr Pattern kWrench{{{{Wave::kFade, kLarge, 170, 0.12f}, {Wave::kPulse, kSmall, 255, 0.05f}}}, 2};
// An enemy killed, a crate broken.
constexpr Pattern kKill{{{{Wave::kFade, kLarge, 120, 0.18f}}}, 1};
// A shot: the Blaster's kick, a bomb's throw, the Devastator's and the R.Y.N.O.'s launch.
constexpr Pattern kShotLight{{{{Wave::kFade, kLarge, 90, 0.08f}}}, 1};
constexpr Pattern kShotMedium{{{{Wave::kFade, kLarge, 170, 0.16f}, {Wave::kPulse, kSmall, 255, 0.06f}}}, 2};
constexpr Pattern kShotHeavy{{{{Wave::kEase, kLarge, 255, 0.35f}, {Wave::kPulse, kSmall, 255, 0.12f}}}, 2};
// Held weapons, kept up while they fire: the Pyrocitor's roar, the Tesla Claw's crackle, the
// Morph-o-Ray's hum (a length is set when each starts and stretched as it goes on).
constexpr Pattern kStreamFlame{{{{Wave::kWobble, kLarge, 35, 0.0f, 0.0f, 0.16f, 45}}}, 1};
constexpr Pattern kStreamTesla{{{{Wave::kWobble, kLarge, 60, 0.0f, 0.0f, 0.05f, 25}}}, 1};
constexpr Pattern kStreamBeam{{{{Wave::kWobble, kLarge, 25, 0.0f, 0.0f, 0.30f, 25}}}, 1};
// On a rail: a low, fast rattle.
constexpr Pattern kGrind{{{{Wave::kWobble, kLarge, 22, 0.0f, 0.0f, 0.12f, 22}}}, 1};

}  // namespace patterns

// --- Hard landings ---

// A landing thuds when he came down for at least kHardDescent seconds and at kHardSpeed units a
// second or faster at some point (a glide on the Heli-Pack comes down for long, but slowly). From
// there to kFullDescent the thud grows from 140 to full.
constexpr float kHardDescent = 0.6f;
constexpr float kFullDescent = 1.5f;
constexpr float kHardSpeed = 12.0f;

// The thud of a landing after `descent` seconds coming down at up to `speed` units a second; false
// for one that is not hard.
inline bool landing_pattern(float descent, float speed, Pattern& out) {
    if (descent < kHardDescent || speed < kHardSpeed) {
        return false;
    }
    const float t = std::clamp((descent - kHardDescent) / (kFullDescent - kHardDescent), 0.0f, 1.0f);
    const auto height = static_cast<std::uint8_t>(std::lround(140.0f + 115.0f * t));
    out = Pattern{{{{Wave::kEase, kLarge, height, 0.22f + 0.25f * t}}}, 1};
    if (t >= 0.35f) {
        out.waves[1] = Wave{Wave::kPulse, kSmall, 255, 0.08f + 0.08f * t};
        out.count = 2;
    }
    return true;
}

// --- Weapons ---

// How a weapon's shot feels, by the game's item id (OpenRAC port/game/rac1/host/figure_tracks.h,
// the save tables).
enum class WeaponClass { kNone, kLight, kMedium, kHeavy, kFlame, kTesla, kBeam };

inline WeaponClass weapon_class(std::int32_t item) {
    switch (item) {
        case 0x09:  // Suck Cannon (its spat enemy)
        case 0x0E:  // Taunter
        case 0x0F:  // Blaster
        case 0x11:  // Mine Glove
        case 0x18:  // Drone Device
        case 0x19:  // Decoy Glove
            return WeaponClass::kLight;
        case 0x0A:  // Bomb Glove
        case 0x0D:  // Visibomb Gun
        case 0x12:  // Walloper
        case 0x14:  // Glove of Doom
            return WeaponClass::kMedium;
        case 0x0B:  // Devastator
        case 0x17:  // R.Y.N.O.
            return WeaponClass::kHeavy;
        case 0x10:  // Pyrocitor
            return WeaponClass::kFlame;
        case 0x13:  // Tesla Claw
            return WeaponClass::kTesla;
        case 0x15:  // Morph-o-Ray
            return WeaponClass::kBeam;
        default:
            return WeaponClass::kNone;
    }
}

// --- One bank of the mixer ---

class Bank {
public:
    Bank() { clear(); }

    void clear() {
        for (auto& w : m_slots) {
            w = L00Entry{};
        }
        m_tags.fill(0);
    }

    // Starts `pattern`'s waves, each in a free slot, else in the one with the least left. `tag`
    // names them for stretch() (0: none).
    void start(const Pattern& pattern, float tick, int tag = 0) {
        for (int i = 0; i < pattern.count; ++i) {
            const int slot = free_slot();
            m_slots[slot] = entry(pattern.waves[i], tick);
            m_tags[slot] = tag;
        }
    }

    // Makes the waves tagged `tag` play for at least `ticks` more; false when none plays.
    bool stretch(int tag, int ticks) {
        bool found = false;
        for (int i = 0; i < ACTUATOR_SLOTS; ++i) {
            L00Entry& w = m_slots[i];
            if (!sounds(w) || m_tags[i] != tag) {
                continue;
            }
            found = true;
            if (static_cast<std::int16_t>(w.f06) < ticks) {
                w.f06 = static_cast<std::uint16_t>(ticks);
            }
            // The phase is an s16: kept within its period so a long hold never wraps it.
            const int period = w.f0a + w.f0c;
            if (period > 0) {
                w.f08 = static_cast<std::int16_t>(w.f08 % period);
            }
        }
        return found;
    }

    // One tick of the game's mixer over the bank: the small and the large motor's values.
    std::array<int, 2> mix() {
        // The game's mixer empties a slot on the tick after its last, which sounds nothing but
        // still counts in its motor's average (a one-tick dip of the others): emptied here first.
        for (int i = 0; i < ACTUATOR_SLOTS; ++i) {
            if (!sounds(m_slots[i])) {
                m_slots[i].f00 = 0;
            }
        }
        s32 out[2] = {0, 0};
        actuator_CalcPower(m_slots.data(), out);
        for (int i = 0; i < ACTUATOR_SLOTS; ++i) {
            if (m_slots[i].f00 == 0) {
                m_tags[i] = 0;
            }
        }
        return {std::clamp(out[0], 0, 255), std::clamp(out[1], 0, 255)};
    }

    bool playing() const {
        return std::any_of(m_slots.begin(), m_slots.end(), [](const L00Entry& w) { return sounds(w); });
    }

    const std::array<L00Entry, ACTUATOR_SLOTS>& slots() const { return m_slots; }

    // `seconds` as ticks of `tick` seconds, at least 1.
    static int ticks(float seconds, float tick) {
        return std::max(1, static_cast<int>(std::lround(seconds / tick)));
    }

private:
    // Whether a slot has ticks left to sound.
    static bool sounds(const L00Entry& w) { return w.f00 != 0 && static_cast<std::int16_t>(w.f06) > 0; }

    int free_slot() const {
        int best = 0;
        for (int i = 0; i < ACTUATOR_SLOTS; ++i) {
            if (!sounds(m_slots[i])) {
                return i;
            }
            if (static_cast<std::int16_t>(m_slots[i].f06) < static_cast<std::int16_t>(m_slots[best].f06)) {
                best = i;
            }
        }
        return best;
    }

    // The mixer's slot for `w`. The mixer counts f06 down a tick at a time, the delay's ticks
    // among them, and the slot sounds while it is above 0 (mix() empties it then); its phase is 1 on
    // the first tick that sounds.
    static L00Entry entry(const Wave& w, float tick) {
        const int delay = w.delay > 0.0f ? ticks(w.delay, tick) : 0;
        const int length = ticks(w.length, tick);
        L00Entry e{};
        e.f02 = w.motor;
        e.f04 = static_cast<s16>(delay);
        e.f06 = static_cast<u16>(delay + length);
        e.f0e = w.height;
        e.f0f = w.floor;
        switch (w.kind) {
            case Wave::kPulse:
                // High for the whole period: phases 1 .. length are all below f0a.
                e.f00 = kSquare;
                e.f0a = static_cast<s16>(length + 1);
                e.f0c = 1;
                break;
            case Wave::kFade:
                // height * (length + 1 - phase) / (length + 1): from just under height to a step
                // above nothing, never back up.
                e.f00 = kSawDown;
                e.f0a = 0;
                e.f0c = static_cast<s16>(length + 1);
                break;
            case Wave::kEase:
                // (cos(pi * phase / f0c) + 1) / 2 * height, phases 1 .. length of f0c = length + 1.
                e.f00 = kCosine;
                e.f0a = 1;
                e.f0c = static_cast<s16>(length + 1);
                break;
            case Wave::kWobble: {
                const int half = std::max(1, ticks(w.period, tick) / 2);
                e.f00 = kTriangle;
                e.f0a = static_cast<s16>(half);
                e.f0c = static_cast<s16>(half);
                break;
            }
        }
        return e;
    }

    std::array<L00Entry, ACTUATOR_SLOTS> m_slots{};
    std::array<int, ACTUATOR_SLOTS> m_tags{};
};

// --- The rumble ---

// The manifest's settings.
struct Settings {
    int strength = 100;  // percent
    bool damage = true;
    bool landings = true;
    bool melee = true;
    bool weapons = true;
    bool explosions = true;
    bool grind = true;
};

// What the game is doing this tick (game.hpp reads it).
struct Frame {
    // A level in play, the player in control: not loading, paused, in a menu or a scene.
    bool playing = false;
    // The hero dying or dead.
    bool dead = false;
    // On a rail.
    bool grinding = false;
    // Feet off the ground.
    bool airborne = false;
    // Where he is takes no thud from a landing (water, a ledge caught, as Clank).
    bool soft = false;
    // His speed downward, units a second (0 going up).
    float fall_speed = 0.0f;
    // Seconds a tick.
    float tick = 1.0f / 60.0f;
    // The camera's shake now (the game's eye offset, in units): the stronger channel's
    // amplitude * (frames / frames_max)^2.
    float shake = 0.0f;
};

// What to send to the motors after a tick: nothing (send false), or small 0/1 and large 0..255.
struct Motors {
    bool send = false;
    int small = 0;
    int large = 0;
};

// The camera's shake as motor values: none below kShakeFloor, full at kShakeFull (the game's
// explosion shakes 0.1 times its size, for a third of a second), the small motor too from kShakeKick.
constexpr float kShakeFloor = 0.005f;
constexpr float kShakeFull = 0.15f;
constexpr float kShakeKick = 0.12f;

// A stream weapon's rumble lasts this long after its last shot unless the game takes ammo again
// sooner: the game takes it every few ticks while the trigger is held (the Pyrocitor's interval is a
// level variable), so the hold is stretched to 1.5 times the gap seen between two takes, within
// these bounds.
constexpr float kStreamHoldFirst = 0.30f;
constexpr float kStreamHoldMin = 0.15f;
constexpr float kStreamHoldMax = 0.60f;

class Rumble {
public:
    void configure(const Settings& settings) { m_settings = settings; }

    const Settings& settings() const { return m_settings; }

    // The game's tick until the first frame says otherwise.
    void set_tick(float seconds) { m_tick = seconds; }

    // The hero took a hit of `amount` (the game's damage, before it takes 1 hit point at most).
    void hurt(int amount) {
        if (amount <= 0 || !taking() || !m_settings.damage) {
            return;
        }
        m_events.start(amount >= 2 ? patterns::kHurtHeavy : patterns::kHurt, m_tick);
    }

    // The wrench connected with something.
    void wrench_hit() {
        if (taking() && m_settings.melee) {
            m_events.start(patterns::kWrench, m_tick);
        }
    }

    // An enemy died, a crate broke.
    void kill() {
        if (taking() && m_settings.melee) {
            m_events.start(patterns::kKill, m_tick);
        }
    }

    // The game took ammo for a shot of `item`.
    void weapon_fired(std::int32_t item) {
        if (!taking() || !m_settings.weapons) {
            return;
        }
        switch (weapon_class(item)) {
            case WeaponClass::kNone:
                return;
            case WeaponClass::kLight:
                m_events.start(patterns::kShotLight, m_tick);
                return;
            case WeaponClass::kMedium:
                m_events.start(patterns::kShotMedium, m_tick);
                return;
            case WeaponClass::kHeavy:
                m_events.start(patterns::kShotHeavy, m_tick);
                return;
            case WeaponClass::kFlame:
                stream(item, patterns::kStreamFlame);
                return;
            case WeaponClass::kTesla:
                stream(item, patterns::kStreamTesla);
                return;
            case WeaponClass::kBeam:
                stream(item, patterns::kStreamBeam);
                return;
        }
    }

    // After the game's tick: plays the waves for `frame` and says what to send.
    Motors tick(const Frame& frame) {
        if (frame.tick > 0.0f) {
            m_tick = frame.tick;
        }
        ++m_ticks;
        m_playing = frame.playing;
        if (!frame.playing) {
            // Paused, a scene, loading: everything stops, and nothing waits to go on after.
            silence();
            return stop();
        }
        if (frame.dead != m_dead) {
            m_dead = frame.dead;
            silence();
            if (m_dead && m_settings.damage) {
                m_events.start(patterns::kDeath, m_tick);
            }
        }
        if (!m_dead) {
            land(frame);
            if (frame.grinding && m_settings.grind && !m_continuous.stretch(kGrindTag, 2)) {
                m_continuous.start(patterns::kGrind, m_tick, kGrindTag);
                m_continuous.stretch(kGrindTag, 2);
            }
        }
        const auto events = m_events.mix();
        const auto continuous = m_continuous.mix();
        int small = std::max(events[kSmall], continuous[kSmall]);
        int large = std::max(events[kLarge], continuous[kLarge]);
        if (!m_dead && m_settings.explosions && frame.shake > kShakeFloor) {
            large = std::max(large, static_cast<int>(std::lround(std::min(frame.shake / kShakeFull, 1.0f) * 255.0f)));
            if (frame.shake >= kShakeKick) {
                small = 255;
            }
        }
        const int strength = std::clamp(m_settings.strength, 0, 100);
        Motors out;
        out.large = (large * strength + 50) / 100;
        out.small = small * strength >= kSmallOn * 100 ? 1 : 0;
        if (out.large == 0 && out.small == 0) {
            return stop();
        }
        out.send = true;
        m_sent = true;
        return out;
    }

    // Whether the last tick sent motors that are still on.
    bool rumbling() const { return m_sent; }

    const Bank& events() const { return m_events; }
    const Bank& continuous() const { return m_continuous; }

private:
    static constexpr int kGrindTag = 1;
    // A stream weapon's tag: its item, past the grind's.
    static constexpr int kStreamTag = 0x100;

    // Events count only in play, with the hero alive.
    bool taking() const { return m_playing && !m_dead; }

    void silence() {
        m_events.clear();
        m_continuous.clear();
        m_air = false;
        m_descent = 0.0f;
        m_peak = 0.0f;
        m_stream_item = -1;
    }

    // 0, 0 once after motors that were on; nothing after that.
    Motors stop() {
        Motors out;
        out.send = m_sent;
        m_sent = false;
        return out;
    }

    // The thud of a hard landing: how long and how fast he came down, measured while he is in the
    // air; on a rail he is on the ground.
    void land(const Frame& frame) {
        const bool air = frame.airborne && !frame.grinding;
        if (air) {
            if (frame.fall_speed > 0.0f) {
                m_descent += m_tick;
                m_peak = std::max(m_peak, frame.fall_speed);
            }
            m_air = true;
            return;
        }
        if (m_air && !frame.soft && m_settings.landings) {
            Pattern thud{};
            if (landing_pattern(m_descent, m_peak, thud)) {
                m_events.start(thud, m_tick);
            }
        }
        m_air = false;
        m_descent = 0.0f;
        m_peak = 0.0f;
    }

    void stream(std::int32_t item, const Pattern& pattern) {
        float hold = kStreamHoldFirst;
        if (item == m_stream_item) {
            const float gap = static_cast<float>(m_ticks - m_stream_tick) * m_tick;
            if (gap <= kStreamHoldMax) {
                hold = std::clamp(gap * 1.5f, kStreamHoldMin, kStreamHoldMax);
            }
        }
        m_stream_item = item;
        m_stream_tick = m_ticks;
        const int tag = kStreamTag + item;
        const int ticks = Bank::ticks(hold, m_tick);
        if (!m_continuous.stretch(tag, ticks)) {
            m_continuous.start(pattern, m_tick, tag);
            m_continuous.stretch(tag, ticks);
        }
    }

    Settings m_settings;
    Bank m_events;
    Bank m_continuous;
    float m_tick = 1.0f / 60.0f;
    std::uint32_t m_ticks = 0;
    bool m_playing = false;
    bool m_dead = false;
    bool m_sent = false;
    // The landing's measure.
    bool m_air = false;
    float m_descent = 0.0f;
    float m_peak = 0.0f;
    // The last stream weapon's take of ammo.
    std::int32_t m_stream_item = -1;
    std::uint32_t m_stream_tick = 0;
};

}  // namespace rumble
