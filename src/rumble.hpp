// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Henning Berge
//
// The rumble: what the game does turned into the pad's two motors, a tick at a time, in the style of
// Going Commando (which drives them; Ratchet & Clank never does). Nothing here touches the game: the
// hooks report events (a hit taken, a shot, a wrench hit, a kill), each tick reports a Frame (what
// the game is doing, game.hpp reads it), and tick() answers what to send.
//
// The waves play on Going Commando's own mixer, LevelActuatorCalcPower (actuator.c, copied; Ratchet
// & Clank's actuator_CalcPower with permanent channels), in two banks of its eight channels: one
// for the short events, each wave counting its duration down, and one for what lasts (a stream
// weapon held, a rail ground, the Hydro-Pack's dash, a pack's hover), a permanent wave started as it starts and emptied as it stops. The
// mixer averages the waves on a motor, so a buzz in the events' bank would halve a hit; the two
// banks and the camera's shake are put together by the stronger instead.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

#include "actuator.h"

namespace rumble {

// The mixer's motors (a channel's output) and shapes (its kind).
constexpr std::uint8_t kSmall = 0;
constexpr std::uint8_t kLarge = 1;
enum Shape : unsigned char { kSquare = 1, kTriangle = 2, kSawUp = 3, kSawDown = 4, kCosine = 5 };

// The small motor is on or off: on while its mixed value, times the strength, is at least this.
constexpr int kSmallOn = 96;

// One wave of a pattern, in seconds (made a channel of ticks as it starts, at the game's tick); the
// fields after the kind are the channel's own, by their names in Going Commando's (WaveChannel).
//   kind  kPulse: flat at `amplitude` for `duration`
//         kFade: a falling saw from `amplitude` to nothing over `duration`
//         kEase: a half cosine from `amplitude` to nothing over `duration` (softer at both ends)
//         kLoop: a permanent triangle between `bias` and `bias + amplitude`, `period` a cycle,
//                until it is stopped (`duration` unused)
struct Wave {
    enum Kind : std::uint8_t { kPulse, kFade, kEase, kLoop } kind;
    std::uint8_t output;
    std::uint8_t amplitude;
    float duration;
    float delay = 0.0f;
    float period = 0.0f;
    std::uint8_t bias = 0;
};

// Up to three waves started together.
struct Pattern {
    std::array<Wave, 3> waves;
    int count;
};

// --- The patterns (seconds; amplitudes 0 to 255) ---

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
// Held weapons, permanent while they fire: the Pyrocitor's roar, the Tesla Claw's crackle, the
// Morph-o-Ray's hum.
constexpr Pattern kStreamFlame{{{{Wave::kLoop, kLarge, 35, 0.0f, 0.0f, 0.16f, 45}}}, 1};
constexpr Pattern kStreamTesla{{{{Wave::kLoop, kLarge, 60, 0.0f, 0.0f, 0.05f, 25}}}, 1};
constexpr Pattern kStreamBeam{{{{Wave::kLoop, kLarge, 25, 0.0f, 0.0f, 0.30f, 25}}}, 1};
// On a rail, permanent while he grinds: a low, fast rattle.
constexpr Pattern kGrind{{{{Wave::kLoop, kLarge, 22, 0.0f, 0.0f, 0.12f, 22}}}, 1};
// The Hydro-Pack's dash, permanent while it lasts (Going Commando plays its pattern 22 each tick
// of its own): a steady push.
constexpr Pattern kSwimDash{{{{Wave::kLoop, kLarge, 30, 0.0f, 0.0f, 0.20f, 40}}}, 1};
// Held up by the Thruster-Pack or the Heli-Pack: a faint hum (Going Commando has none).
constexpr Pattern kHover{{{{Wave::kLoop, kLarge, 10, 0.0f, 0.0f, 0.25f, 18}}}, 1};
// A pack's jump starting (the Thruster-Pack's long jump; Going Commando's pattern 20): a kick.
constexpr Pattern kPackJump{{{{Wave::kFade, kLarge, 150, 0.15f}, {Wave::kPulse, kSmall, 255, 0.05f}}}, 2};
// A wall jump (Going Commando's pattern 27): a lighter kick.
constexpr Pattern kWallJump{{{{Wave::kFade, kLarge, 120, 0.12f}}}, 1};
// The wrench slam striking the ground (Going Commando's pattern 25, with a camera shake): a thump,
// between a hit taken and the heaviest shot.
constexpr Pattern kSlam{{{{Wave::kEase, kLarge, 230, 0.30f}, {Wave::kPulse, kSmall, 255, 0.10f}}}, 2};

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
    const auto amplitude = static_cast<std::uint8_t>(std::lround(140.0f + 115.0f * t));
    out = Pattern{{{{Wave::kEase, kLarge, amplitude, 0.22f + 0.25f * t}}}, 1};
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
        for (auto& c : m_channels) {
            c = WaveChannel{};
        }
        m_tags.fill(0);
    }

    // Starts `pattern`'s waves, each in a free channel, else in the one with the least left (a
    // permanent one last). `tag` names them for has() and stop() (0: none).
    void start(const Pattern& pattern, float tick, int tag = 0) {
        for (int i = 0; i < pattern.count; ++i) {
            const int at = free_channel();
            m_channels[at] = channel(pattern.waves[i], tick);
            m_tags[at] = tag;
        }
    }

    // Whether a wave tagged `tag` plays.
    bool has(int tag) const {
        for (int i = 0; i < WAVE_CHANNELS; ++i) {
            if (m_tags[i] == tag && sounds(m_channels[i])) {
                return true;
            }
        }
        return false;
    }

    // Empties the channels of the waves tagged `tag`.
    void stop(int tag) {
        for (int i = 0; i < WAVE_CHANNELS; ++i) {
            if (m_tags[i] == tag) {
                m_channels[i] = WaveChannel{};
                m_tags[i] = 0;
            }
        }
    }

    // One tick of the game's mixer over the bank: the small and the large motor's values.
    std::array<int, 2> mix() {
        // The game's mixer empties a counted-down channel on the tick after its last, which sounds
        // nothing but still counts in its motor's average (a one-tick dip of the others): emptied
        // here first.
        for (auto& c : m_channels) {
            if (!sounds(c)) {
                c.kind = 0;
            }
        }
        int out[2] = {0, 0};
        LevelActuatorCalcPower(m_channels.data(), out);
        for (int i = 0; i < WAVE_CHANNELS; ++i) {
            WaveChannel& c = m_channels[i];
            if (c.kind == 0) {
                m_tags[i] = 0;
            } else if (c.permanent != 0 && c.rise + c.fall > 0) {
                // A permanent wave's frame (a short) would wrap after nine minutes or so: kept
                // within its period, which leaves its phase as it is.
                c.frame = static_cast<short>(c.frame % (c.rise + c.fall));
            }
        }
        return {std::clamp(out[0], 0, 255), std::clamp(out[1], 0, 255)};
    }

    bool playing() const {
        return std::any_of(m_channels.begin(), m_channels.end(), [](const WaveChannel& c) { return sounds(c); });
    }

    const std::array<WaveChannel, WAVE_CHANNELS>& channels() const { return m_channels; }

    // `seconds` as ticks of `tick` seconds, at least 1.
    static int ticks(float seconds, float tick) {
        return std::max(1, static_cast<int>(std::lround(seconds / tick)));
    }

private:
    // Whether a channel has ticks left to sound: a permanent one until it is emptied.
    static bool sounds(const WaveChannel& c) { return c.kind != 0 && (c.permanent != 0 || c.duration > 0); }

    int free_channel() const {
        int best = -1;
        for (int i = 0; i < WAVE_CHANNELS; ++i) {
            const WaveChannel& c = m_channels[i];
            if (!sounds(c)) {
                return i;
            }
            if (c.permanent == 0 && (best < 0 || c.duration < m_channels[best].duration)) {
                best = i;
            }
        }
        return best < 0 ? 0 : best;
    }

    // The mixer's channel for `w`. The mixer counts duration down a tick at a time, the delay's
    // ticks among them, and the channel sounds while it is above 0 (mix() empties it then); a
    // permanent channel is never counted down. Its frame is 1 on the first tick that sounds.
    static WaveChannel channel(const Wave& w, float tick) {
        const int delay = w.delay > 0.0f ? ticks(w.delay, tick) : 0;
        const int duration = ticks(w.duration, tick);
        WaveChannel c{};
        c.output = w.output;
        c.delay = static_cast<short>(delay);
        c.duration = static_cast<short>(delay + duration);
        c.amplitude = w.amplitude;
        c.bias = w.bias;
        switch (w.kind) {
            case Wave::kPulse:
                // High for the whole period: frames 1 .. duration are all below rise.
                c.kind = kSquare;
                c.rise = static_cast<short>(duration + 1);
                c.fall = 1;
                break;
            case Wave::kFade:
                // amplitude * (duration + 1 - frame) / (duration + 1): from just under amplitude
                // to a step above nothing, never back up.
                c.kind = kSawDown;
                c.rise = 0;
                c.fall = static_cast<short>(duration + 1);
                break;
            case Wave::kEase:
                // (cos(pi * frame / fall) + 1) / 2 * amplitude, frames 1 .. duration of
                // fall = duration + 1.
                c.kind = kCosine;
                c.rise = 1;
                c.fall = static_cast<short>(duration + 1);
                break;
            case Wave::kLoop: {
                const int half = std::max(1, ticks(w.period, tick) / 2);
                c.kind = kTriangle;
                c.permanent = 1;
                c.rise = static_cast<short>(half);
                c.fall = static_cast<short>(half);
                break;
            }
        }
        return c;
    }

    std::array<WaveChannel, WAVE_CHANNELS> m_channels{};
    std::array<int, WAVE_CHANNELS> m_tags{};
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
    bool jumps = true;
    bool hover = false;
};

// What the game is doing this tick (game.hpp reads it).
struct Frame {
    // A level in play, the player in control: not loading, paused, in a menu or a scene.
    bool playing = false;
    // The hero dying or dead.
    bool dead = false;
    // On a rail.
    bool grinding = false;
    // Held up by the Thruster-Pack's hover or the Heli-Pack's glide.
    bool hovering = false;
    // The Hydro-Pack's dash under water.
    bool swim_dash = false;
    // The wrench slam struck the ground this tick.
    bool slam = false;
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

// A stream weapon has no event for its trigger let go: the game takes its ammo every few ticks while
// it fires (the Pyrocitor's interval is a level variable). Its permanent wave stops when no take came
// for 1.5 times the gap seen between the last two, within these bounds (kStreamHoldFirst after the
// first take).
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

    // The hero entered move state `state` (enter_move_state took it): the pack's jumps and the wall
    // jump kick.
    void jumped(std::int32_t state) {
        if (!taking() || !m_settings.jumps) {
            return;
        }
        if (state == 0xD || state == 0x10) {
            m_events.start(patterns::kPackJump, m_tick);
        } else if (state == 0x11) {
            m_events.start(patterns::kWallJump, m_tick);
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
            if (frame.slam && m_settings.melee) {
                m_events.start(patterns::kSlam, m_tick);
            }
            hold(kGrindTag, frame.grinding && m_settings.grind, patterns::kGrind);
            hold(kSwimTag, frame.swim_dash && m_settings.grind, patterns::kSwimDash);
            hold(kHoverTag, frame.hovering && m_settings.hover, patterns::kHover);
            if (m_stream_item >= 0 && (!m_settings.weapons || m_ticks - m_stream_tick > m_stream_hold)) {
                end_stream();
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
    static constexpr int kSwimTag = 2;
    static constexpr int kHoverTag = 3;
    // A stream weapon's tag: its item, past the others.
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

    // A permanent wave for as long as `on` holds: started the tick it turns on, stopped the tick
    // it turns off.
    void hold(int tag, bool on, const Pattern& pattern) {
        if (on && !m_continuous.has(tag)) {
            m_continuous.start(pattern, m_tick, tag);
        } else if (!on) {
            m_continuous.stop(tag);
        }
    }

    void end_stream() {
        m_continuous.stop(kStreamTag + m_stream_item);
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

    // A take of a stream weapon's ammo: its permanent wave started if it is not playing (one held
    // weapon at a time), and how long until it stops without another.
    void stream(std::int32_t item, const Pattern& pattern) {
        float hold = kStreamHoldFirst;
        if (item == m_stream_item) {
            const float gap = static_cast<float>(m_ticks - m_stream_tick) * m_tick;
            hold = std::clamp(gap * 1.5f, kStreamHoldMin, kStreamHoldMax);
        } else if (m_stream_item >= 0) {
            end_stream();
        }
        m_stream_item = item;
        m_stream_tick = m_ticks;
        m_stream_hold = static_cast<std::uint32_t>(Bank::ticks(hold, m_tick));
        if (!m_continuous.has(kStreamTag + item)) {
            m_continuous.start(pattern, m_tick, kStreamTag + item);
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
    // The stream weapon playing (-1: none), its last take of ammo and the ticks it holds after one.
    std::int32_t m_stream_item = -1;
    std::uint32_t m_stream_tick = 0;
    std::uint32_t m_stream_hold = 0;
};

}  // namespace rumble
