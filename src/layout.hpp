// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Henning Berge
//
// Where Ratchet & Clank keeps what this mod reads, on both discs. The hero's field offsets are the
// same on PAL and NTSC; the bases differ (NTSC is not a flat 0x100 below PAL).
//   rac1-decomp include/structs/hero.h (PAL), include/enums/hero_state.h; OpenRAC
//   port/game/rac1/host/camera_override.c and port/game/rac1-ntsc/host/rac1_version.h.
#pragma once

#include <cstdint>

namespace rac1_rumble {

// --- The hero (offsets from g_hero) ---

// Hero.motion.velocity: units a tick (x, y, z; z is up).
constexpr std::uint32_t kVelocityZ = 0xE8u;
// Hero.air_frames: ticks off the ground, 0 on landing (s16).
constexpr std::uint32_t kAirFrames = 0x30Eu;
// Hero.unkA64: the wrench's hit latch. wrench_update sets it to 1 as the swing (or the thrown
// wrench) connects with an enemy, a crate or a wall (with the impact's sparks and sound), and the
// hero's states clear it for the next swing (rac1-decomp bolts_and_wrench.c, wrench_update).
constexpr std::uint32_t kWrenchHit = 0xA64u;
// Hero.items[0].item_id: the item in hand, which use_ammo takes for its id -1
// (g_hero_equipped_item_id, PAL D_00140508).
constexpr std::uint32_t kItemInHand = 0x10B8u;
// Hero.state.current.
constexpr std::uint32_t kHeroState = 0x2084u;
// Hero.vehicle_mode: non-zero as Clank or Giant Clank.
constexpr std::uint32_t kVehicleMode = 0x20A4u;
// Hero.health.hp: 0 is dead.
constexpr std::uint32_t kHealth = 0x22A8u;

// --- The level camera's shake (CameraSystem.shake at +0x160: two CameraShake records of 0x10
// bytes, f32 amplitude, f32 offset, s32 frames, s32 frames_max). apply_camera_shake moves the eye by
// amplitude * cos(2 * frames) * (frames / frames_max)^2 while frames counts down. ---
constexpr std::uint32_t kShakeStride = 0x10u;
constexpr std::uint32_t kShakeAmplitude = 0x0u;
constexpr std::uint32_t kShakeFrames = 0x8u;
constexpr std::uint32_t kShakeFramesMax = 0xCu;

// --- Hero states (include/enums/hero_state.h) ---

// Dead or dying: these and health 0.
constexpr std::int32_t kDyingStates[] = {
    0x3D,  // DYING
    0x47,  // VEHICLE_MODE_1_DYING
    0x57,  // VEHICLE_MODE_3_DYING
    0x6A,  // DROWN
    0x77,  // FATAL_FALL
    0x7C,  // DEATH_DESCENT
    0x7F,  // SINK
    0x80,  // DYING_SPEED_LIMITED
    0x82,  // DYING_ZERO_HEALTH
};

// On a rail.
constexpr std::int32_t kGrindStates[] = {
    0x28,  // GRIND
    0x2A,  // GRIND_RAIL_TRANSFER
    0x2B,  // GRIND_ATTACK
};

// Where a fall ends in no thud: water, a ledge caught.
constexpr std::int32_t kSoftLandingStates[] = {
    0x18, 0x19, 0x1A, 0x1B, 0x1C,  // LEDGE_HANG_ENTRY ... LEDGE_JUMP
    0x4B, 0x4D, 0x4E,              // ALT_LEDGE_HANG, its shimmies
    0x33, 0x34, 0x36, 0x37,        // SWIM, SWIM_IDLE, WATER_SURFACE_MOVE, WATER_SURFACE_ENTRY
    0x75,                          // WATER_SURFACE_HURT
};

template <std::size_t N>
constexpr bool one_of(std::int32_t state, const std::int32_t (&states)[N]) {
    for (std::int32_t s : states) {
        if (s == state) {
            return true;
        }
    }
    return false;
}

struct Disc {
    // g_hero.
    std::uint32_t hero;
    // g_frame_time: seconds a tick.
    std::uint32_t frame_time;
    // The level camera's shake records: level 0's address, which relocate_level(0, ...) moves to
    // the level loaded now (PAL D_L00_00166EE0; NTSC 0x00166DE0, rac1_version.h).
    std::uint32_t shake_l00;
    // Seconds a tick when g_frame_time reads as nothing sensible.
    float tick_seconds;
};

inline Disc disc_for(bool ntsc) {
    if (ntsc) {
        return {0x0013F350u, 0x0015ED6Cu, 0x00166DE0u, 1.0f / 60.0f};
    }
    return {0x0013F450u, 0x0015EE6Cu, 0x00166EE0u, 1.0f / 50.0f};
}

}  // namespace rac1_rumble
