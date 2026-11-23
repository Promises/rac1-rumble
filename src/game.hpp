// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Henning Berge
//
// What the rumble needs of the game each tick (rumble::Frame), read from its memory: the hero's
// state, health, air time and fall, and the level camera's shake. Memory is anything with
// read<T>(address) (openrac::Memory in the game, a buffer in the tests).
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "layout.hpp"
#include "rumble.hpp"

namespace rac1_rumble {

// The game's seconds a tick, or the disc's usual when g_frame_time holds nothing sensible.
template <class Memory>
float tick_seconds(const Memory& m, const Disc& disc) {
    const float t = m.template read<float>(disc.frame_time);
    return std::isfinite(t) && t >= 1.0f / 240.0f && t <= 1.0f / 10.0f ? t : disc.tick_seconds;
}

// The stronger of the camera's two shake channels at `shake` (the level loaded now's records, 0
// for none): what apply_camera_shake moves the eye by this tick, but for its cosine.
template <class Memory>
float camera_shake(const Memory& m, std::uint32_t shake) {
    if (shake == 0) {
        return 0.0f;
    }
    float strongest = 0.0f;
    for (std::uint32_t channel = 0; channel < 2; ++channel) {
        const std::uint32_t at = shake + channel * kShakeStride;
        const float amplitude = std::fabs(m.template read<float>(at + kShakeAmplitude));
        const std::int32_t frames = m.template read<std::int32_t>(at + kShakeFrames);
        const std::int32_t longest = m.template read<std::int32_t>(at + kShakeFramesMax);
        if (frames <= 0 || !std::isfinite(amplitude)) {
            continue;
        }
        // frames_max is set from frames on the first tick it applies; until then the shake is new.
        const float ratio = longest >= frames ? static_cast<float>(frames) / static_cast<float>(longest) : 1.0f;
        strongest = std::max(strongest, std::min(amplitude, 10.0f) * ratio * ratio);
    }
    return strongest;
}

// The frame of a level in play (`playing` false: the rest is not read). `shake` is the level's
// shake records (relocated), 0 for none.
template <class Memory>
rumble::Frame read_frame(const Memory& m, const Disc& disc, bool playing, std::uint32_t shake) {
    rumble::Frame f;
    f.tick = tick_seconds(m, disc);
    f.playing = playing;
    if (!playing) {
        return f;
    }
    const std::uint32_t hero = disc.hero;
    const std::int32_t state = m.template read<std::int32_t>(hero + kHeroState);
    f.dead = m.template read<std::int32_t>(hero + kHealth) <= 0 || one_of(state, kDyingStates);
    f.grinding = one_of(state, kGrindStates);
    f.airborne = m.template read<std::int16_t>(hero + kAirFrames) > 0;
    f.soft = one_of(state, kSoftLandingStates) || m.template read<std::uint8_t>(hero + kVehicleMode) != 0;
    const float vz = m.template read<float>(hero + kVelocityZ);
    f.fall_speed = std::isfinite(vz) && vz < 0.0f ? -vz / f.tick : 0.0f;
    f.shake = camera_shake(m, shake);
    return f;
}

}  // namespace rac1_rumble
