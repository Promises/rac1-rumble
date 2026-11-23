// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Henning Berge
//
// The rumble's rules (rumble.hpp) on the copied mixer (actuator.c), and the game reader (game.hpp)
// on a buffer standing in for game memory. They test what the mod sends for what the game reports;
// how it feels on a pad is for playing.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "game.hpp"
#include "layout.hpp"
#include "rumble.hpp"

namespace {

using rumble::Frame;
using rumble::Motors;
using rumble::Rumble;
using rumble::Settings;
using rumble::WeaponClass;

int failures = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("FAIL: %s\n", what);
        ++failures;
    }
}

constexpr float kTick = 1.0f / 50.0f;

Frame play() {
    Frame f;
    f.playing = true;
    f.tick = kTick;
    return f;
}

// A rumble in play whose load-time 0, 0 reached the port, a first quiet tick behind it (events
// count from then).
Rumble started(const Settings& settings = Settings{}) {
    Rumble r;
    r.configure(settings);
    r.set_tick(kTick);
    r.announced();
    r.tick(play());
    return r;
}

void test_first_call() {
    // The call at load was refused: the first tick sends 0, 0, in play or not, until one is taken.
    Rumble r;
    r.set_tick(kTick);
    Frame menu = play();
    menu.playing = false;
    Motors m = r.tick(menu);
    check(m.send && m.small == 0 && m.large == 0, "refused at load: 0, 0 on the first tick");
    m = r.tick(play());
    check(m.send && m.small == 0 && m.large == 0, "and again while no call has been taken");
    r.announced();
    check(!r.tick(play()).send, "taken: nothing more while quiet");
    // Taken at load: nothing on the first tick.
    Rumble loaded;
    loaded.set_tick(kTick);
    loaded.announced();
    check(!loaded.tick(menu).send && !loaded.tick(play()).send, "taken at load: quiet ticks send nothing");
    // A real rumble counts as the first call too.
    Rumble busy;
    busy.set_tick(kTick);
    busy.tick(play());
    busy.hurt(1);
    m = busy.tick(play());
    check(m.send && m.large > 0, "a hit before any call was taken is sent as it is");
}

// Ticks `r` in `frame` until it goes quiet (at most `limit` ticks): how many ticks sent motors on,
// whether the last send was 0, 0, and the strongest large value sent.
struct Run {
    int on = 0;
    bool stopped = false;
    int peak = 0;
    int ticks = 0;
};

Run run(Rumble& r, const Frame& frame, int limit = 500) {
    Run out;
    for (int i = 0; i < limit; ++i) {
        const Motors m = r.tick(frame);
        ++out.ticks;
        if (!m.send) {
            break;
        }
        if (m.small == 0 && m.large == 0) {
            out.stopped = true;
            break;
        }
        ++out.on;
        out.peak = std::max(out.peak, m.large);
    }
    return out;
}

// --- The mixer ---

void test_mixer_pulse_plays_its_length() {
    rumble::Bank bank;
    // 0.2 s at 50 ticks a second: 10 ticks of 200.
    bank.start(rumble::Pattern{{{{rumble::Wave::kPulse, rumble::kLarge, 200, 0.2f}}}, 1}, kTick);
    int ticks = 0;
    while (bank.playing() && ticks < 100) {
        const auto v = bank.mix();
        check(v[rumble::kLarge] == 200, "a pulse is flat at its height");
        ++ticks;
    }
    check(ticks == 10, "a pulse of 0.2 s plays 10 ticks of 50 a second");
    check(bank.mix()[rumble::kLarge] == 0, "an empty bank mixes to nothing");
}

void test_mixer_fade_and_ease_decay_to_zero() {
    for (auto kind : {rumble::Wave::kFade, rumble::Wave::kEase}) {
        rumble::Bank bank;
        bank.start(rumble::Pattern{{{{kind, rumble::kLarge, 255, 0.3f}}}, 1}, kTick);
        int last = 256;
        bool falling = true;
        int ticks = 0;
        int final_value = -1;
        while (bank.playing() && ticks < 100) {
            const int v = bank.mix()[rumble::kLarge];
            falling = falling && v <= last;
            last = v;
            final_value = v;
            ++ticks;
        }
        check(ticks == 15, "a 0.3 s wave plays 15 ticks");
        check(falling, "a fade or an ease only falls");
        check(final_value < 30, "a fade or an ease ends near nothing");
        check(!bank.playing(), "the slot is free after its length");
    }
}

void test_mixer_delay_waits_and_averages_only_what_plays() {
    rumble::Bank bank;
    // A pulse of 100 for 2 ticks, and one of 200 that waits 2 ticks then plays 2.
    bank.start(rumble::Pattern{{{{rumble::Wave::kPulse, rumble::kLarge, 100, 2 * kTick},
                                 {rumble::Wave::kPulse, rumble::kLarge, 200, 2 * kTick, 2 * kTick}}},
                               2},
               kTick);
    const int a = bank.mix()[rumble::kLarge];
    const int b = bank.mix()[rumble::kLarge];
    const int c = bank.mix()[rumble::kLarge];
    const int d = bank.mix()[rumble::kLarge];
    check(a == 100 && b == 100, "the first pulse alone while the second waits");
    check(c == 200 && d == 200, "the second pulse after its wait");
    check(!bank.playing(), "both done after four ticks");
}

void test_mixer_averages_a_motor() {
    rumble::Bank bank;
    bank.start(rumble::Pattern{{{{rumble::Wave::kPulse, rumble::kLarge, 200, 0.1f},
                                 {rumble::Wave::kPulse, rumble::kLarge, 100, 0.1f},
                                 {rumble::Wave::kPulse, rumble::kSmall, 255, 0.1f}}},
                               3},
               kTick);
    const auto v = bank.mix();
    check(v[rumble::kLarge] == 150, "the game's mixer averages the waves on a motor");
    check(v[rumble::kSmall] == 255, "the other motor apart");
}

void test_mixer_full_bank_takes_the_shortest() {
    rumble::Bank bank;
    // A permanent wave first, then seven that count down: the shortest of those goes, not it.
    bank.start(rumble::patterns::kGrind, kTick, 7);
    for (int i = 1; i < WAVE_CHANNELS; ++i) {
        bank.start(rumble::Pattern{{{{rumble::Wave::kPulse, rumble::kLarge, 10, 0.1f * i}}}, 1}, kTick);
    }
    bank.start(rumble::Pattern{{{{rumble::Wave::kPulse, rumble::kLarge, 250, 1.0f}}}, 1}, kTick);
    int found = 0;
    int shortest = 1000;
    for (const auto& c : bank.channels()) {
        found += c.amplitude == 250 ? 1 : 0;
        if (c.permanent == 0) {
            shortest = std::min(shortest, static_cast<int>(c.duration));
        }
    }
    check(found == 1, "a full bank still starts a new wave");
    check(shortest == 10, "in place of the one with the least left (0.1 s: 5 ticks, gone)");
    check(bank.has(7), "a permanent wave is never the one taken");
}

void test_mixer_permanent_plays_until_stopped() {
    rumble::Bank bank;
    bank.start(rumble::patterns::kGrind, kTick, 7);
    // Ten minutes at 50 ticks a second: past a short's range of frames.
    bool sounding = true;
    for (int i = 0; i < 30000; ++i) {
        sounding = sounding && bank.mix()[rumble::kLarge] >= rumble::patterns::kGrind.waves[0].bias;
    }
    check(sounding && bank.has(7) && bank.playing(), "a permanent wave keeps going, never counted down");
    for (const auto& c : bank.channels()) {
        if (c.kind != 0) {
            check(c.permanent != 0 && c.frame >= 0 && c.frame <= c.rise + c.fall, "its frame never wraps");
        }
    }
    bank.stop(7);
    check(!bank.has(7) && !bank.playing() && bank.mix()[rumble::kLarge] == 0, "stopped, it is gone at once");
}

void test_mixer_counted_wave_ends() {
    rumble::Bank bank;
    bank.start(rumble::Pattern{{{{rumble::Wave::kFade, rumble::kLarge, 200, 0.1f}}}, 1}, kTick, 3);
    check(bank.channels()[0].permanent == 0 && bank.channels()[0].duration == 5, "a fade counts 5 ticks down");
    for (int i = 0; i < 5; ++i) {
        check(bank.mix()[rumble::kLarge] > 0, "and sounds for them");
    }
    check(!bank.has(3) && !bank.playing(), "then it is gone by itself");
}

// --- The rumble ---

void test_every_pattern_ends() {
    const rumble::Pattern* all[] = {
        &rumble::patterns::kHurt,       &rumble::patterns::kHurtHeavy,  &rumble::patterns::kDeath,
        &rumble::patterns::kWrench,     &rumble::patterns::kKill,       &rumble::patterns::kShotLight,
        &rumble::patterns::kShotMedium, &rumble::patterns::kShotHeavy,  &rumble::patterns::kPackJump,
        &rumble::patterns::kWallJump,   &rumble::patterns::kSlam,
    };
    for (const auto* p : all) {
        for (float tick : {1.0f / 50.0f, 1.0f / 60.0f}) {
            rumble::Bank bank;
            bank.start(*p, tick);
            int ticks = 0;
            while (bank.playing() && ticks < 1000) {
                bank.mix();
                ++ticks;
            }
            check(!bank.playing() && ticks < 70, "each event's pattern ends within about a second");
            const auto v = bank.mix();
            check(v[0] == 0 && v[1] == 0, "and leaves the motors at nothing");
        }
    }
}

void test_sends_every_tick_then_stops_once() {
    Rumble r = started();
    r.hurt(1);
    const Run out = run(r, play());
    // kHurt: 0.35 s of the large motor at 50 ticks a second.
    check(out.on >= 15 && out.on <= 18, "motors sent each tick of the hit");
    check(out.stopped, "then 0, 0 once");
    check(out.peak >= 180, "a hit is strong");
    const Motors after = r.tick(play());
    check(!after.send, "and nothing after the stop");
}

void test_heavier_hits_are_stronger() {
    Rumble light = started();
    light.hurt(1);
    const Run a = run(light, play());
    Rumble heavy = started();
    heavy.hurt(3);
    const Run b = run(heavy, play());
    check(b.peak > a.peak && b.on > a.on, "a heavier hit is stronger and longer");
    Rumble none = started();
    none.hurt(0);
    check(!none.tick(play()).send, "no damage, no rumble");
}

void test_strength_scales() {
    Settings half;
    half.strength = 50;
    Rumble full = started();
    full.hurt(1);
    const Motors f = full.tick(play());
    Rumble r = started(half);
    r.hurt(1);
    const Motors h = r.tick(play());
    check(std::abs(h.large * 2 - f.large) <= 2, "50% halves the large motor");
    check(f.small == 1 && h.small == 1, "the small motor stays on at 50%");
    Settings low;
    low.strength = 30;
    Rumble l = started(low);
    l.hurt(1);
    check(l.tick(play()).small == 0, "the small motor drops out at 30%");
    Settings off;
    off.strength = 0;
    Rumble o = started(off);
    o.hurt(3);
    check(!o.tick(play()).send, "0% sends nothing");
}

void test_pause_stops_and_forgets() {
    Rumble r = started();
    r.hurt(3);
    check(r.tick(play()).send, "rumbling in play");
    Frame paused = play();
    paused.playing = false;
    const Motors stop = r.tick(paused);
    check(stop.send && stop.small == 0 && stop.large == 0, "a pause sends 0, 0");
    check(!r.tick(paused).send, "once");
    r.hurt(3);
    check(!r.tick(paused).send, "a hit reported in the menu rumbles not");
    check(!r.tick(play()).send, "and nothing comes back after it");
}

void test_events_before_play_are_dropped() {
    Rumble r;
    r.set_tick(kTick);
    r.announced();
    r.hurt(1);
    r.weapon_fired(0x0F);
    check(!r.tick(play()).send, "nothing before the first tick in play");
}

void test_death_jolts_then_silence() {
    Rumble r = started();
    Frame dead = play();
    dead.dead = true;
    dead.shake = 0.2f;
    const Run out = run(r, dead);
    check(out.on >= 40 && out.stopped, "dying: a long jolt, then 0, 0");
    r.hurt(1);
    r.weapon_fired(0x0B);
    r.wrench_hit();
    check(!r.tick(dead).send, "while dead nothing rumbles, the camera's shake neither");
    check(!r.tick(play()).send, "alive again: quiet");
    r.hurt(1);
    check(r.tick(play()).send, "then hits rumble again");
    Settings no_damage;
    no_damage.damage = false;
    Rumble q = started(no_damage);
    check(!q.tick(dead).send, "no jolt with hits taken off");
}

void test_toggles() {
    Settings s;
    s.damage = s.melee = s.weapons = s.explosions = s.grind = s.jumps = s.hover = false;
    Rumble r = started(s);
    r.hurt(3);
    r.wrench_hit();
    r.kill();
    r.weapon_fired(0x0B);
    r.weapon_fired(0x10);
    r.jumped(0x10);
    Frame f = play();
    f.shake = 0.3f;
    f.grinding = true;
    f.swim_dash = true;
    f.hovering = true;
    f.slam = true;
    check(!r.tick(f).send, "every toggle off: nothing");
    Rumble on = started();
    on.wrench_hit();
    check(on.tick(play()).send, "the wrench rumbles");
    Rumble kill = started();
    kill.kill();
    check(kill.tick(play()).send, "a kill rumbles");
}

void test_weapon_classes() {
    check(rumble::weapon_class(0x0F) == WeaponClass::kLight, "Blaster: light");
    check(rumble::weapon_class(0x0A) == WeaponClass::kMedium, "Bomb Glove: medium");
    check(rumble::weapon_class(0x0B) == WeaponClass::kHeavy, "Devastator: heavy");
    check(rumble::weapon_class(0x17) == WeaponClass::kHeavy, "R.Y.N.O.: heavy");
    check(rumble::weapon_class(0x10) == WeaponClass::kFlame, "Pyrocitor: flame");
    check(rumble::weapon_class(0x13) == WeaponClass::kTesla, "Tesla Claw: tesla");
    check(rumble::weapon_class(0x15) == WeaponClass::kBeam, "Morph-o-Ray: beam");
    check(rumble::weapon_class(0x08) == WeaponClass::kNone, "the wrench is no shot");
    check(rumble::weapon_class(0x02) == WeaponClass::kNone, "nor the Heli-Pack");
    check(rumble::weapon_class(-1) == WeaponClass::kNone, "nor nothing");

    Rumble light = started();
    light.weapon_fired(0x0F);
    const Run a = run(light, play());
    Rumble heavy = started();
    heavy.weapon_fired(0x0B);
    const Run b = run(heavy, play());
    check(a.on > 0 && a.on <= 5 && a.peak < 120, "a Blaster shot: a short, light kick");
    check(b.on > 10 && b.peak >= 240, "a Devastator shot: long and full");
}

void test_stream_holds_while_firing() {
    Rumble r = started();
    int on = 0;
    // The game takes the Pyrocitor's ammo every 6 ticks for 2 seconds.
    for (int i = 0; i < 100; ++i) {
        if (i % 6 == 0) {
            r.weapon_fired(0x10);
        }
        const Motors m = r.tick(play());
        on += m.send && m.large > 0 ? 1 : 0;
    }
    check(on == 100, "a held Pyrocitor buzzes without a gap");
    const Run tail = run(r, play());
    check(tail.stopped && tail.on <= 10, "and stops soon after its last take (1.5 gaps)");
    check(tail.peak <= 90, "a low buzz");
}

void test_continuous_does_not_halve_a_hit() {
    Rumble r = started();
    r.weapon_fired(0x10);
    r.tick(play());
    r.weapon_fired(0x10);
    r.hurt(3);
    const Motors m = r.tick(play());
    Rumble alone = started();
    alone.hurt(3);
    const Motors a = alone.tick(play());
    check(m.large == a.large, "a hit while the flamethrower buzzes is as strong as alone");
}

void test_grind() {
    Rumble r = started();
    Frame f = play();
    f.grinding = true;
    int on = 0;
    for (int i = 0; i < 120; ++i) {
        const Motors m = r.tick(f);
        on += m.send && m.large > 0 ? 1 : 0;
    }
    check(on == 120, "a rail rattles each tick");
    const Run tail = run(r, play());
    check(tail.stopped && tail.on == 0, "off the rail it stops at once");
}

void test_jumps() {
    Rumble long_jump = started();
    long_jump.jumped(0x10);
    const Run a = run(long_jump, play());
    check(a.on >= 6 && a.on <= 9 && a.peak >= 130 && a.stopped, "the Thruster-Pack's long jump: a short kick");
    Rumble flip = started();
    flip.jumped(0xD);
    check(run(flip, play()).peak == a.peak, "state 0xD: the same kick (Going Commando's pattern 20 for both)");
    Rumble wall = started();
    wall.jumped(0x11);
    const Run w = run(wall, play());
    check(w.on > 0 && w.peak < a.peak, "a wall jump: a lighter kick");
    for (std::int32_t plain : {0x7, 0x9, 0xA, 0xB, 0xE, 0x8, 0x81}) {
        Rumble r = started();
        r.jumped(plain);
        check(!r.tick(play()).send, "an ordinary jump, a double jump, a pack's glide: no kick");
    }
    Settings off;
    off.jumps = false;
    Rumble o = started(off);
    o.jumped(0x10);
    check(!o.tick(play()).send, "jumps off: no kick");
    Rumble paused;
    paused.set_tick(kTick);
    paused.announced();
    Frame menu = play();
    menu.playing = false;
    paused.tick(menu);
    paused.jumped(0x10);
    check(!paused.tick(play()).send, "a jump reported out of play: no kick");
    Rumble dead = started();
    Frame d = play();
    d.dead = true;
    run(dead, d);
    dead.jumped(0x10);
    check(!dead.tick(d).send, "dead: no kick");
}

void test_slam() {
    Rumble r = started();
    Frame f = play();
    f.slam = true;
    const Motors first = r.tick(f);
    const Run rest = run(r, play());
    check(first.send && first.small == 1 && rest.peak >= 200 && rest.on >= 12 && rest.stopped,
          "the wrench slam striking: a thump on both motors");
    Settings off;
    off.melee = false;
    Rumble o = started(off);
    check(!o.tick(f).send, "wrench and kills off: no thump");
}

// Ticks `r` `ticks` times in `frame`; how many sent the large motor on.
int held(Rumble& r, const Frame& frame, int ticks) {
    int on = 0;
    for (int i = 0; i < ticks; ++i) {
        const Motors m = r.tick(frame);
        on += m.send && m.large > 0 ? 1 : 0;
    }
    return on;
}

void test_hover() {
    Frame f = play();
    f.hovering = true;
    Rumble quiet = started();
    check(held(quiet, f, 50) == 0, "the hover hum is off by default");
    Settings on;
    on.hover = true;
    Rumble r = started(on);
    check(held(r, f, 600) == 600, "on: it hums every tick the pack holds him up");
    const Motors left = r.tick(play());
    check(left.send && left.large == 0 && left.small == 0, "and stops the tick he leaves it");
    check(!r.tick(play()).send, "once");
    Rumble faint = started(on);
    int peak = 0;
    for (int i = 0; i < 50; ++i) {
        peak = std::max(peak, faint.tick(f).large);
    }
    check(peak > 0 && peak <= 30, "a faint hum, never the small motor");
    Rumble with_hit = started(on);
    with_hit.tick(f);
    with_hit.hurt(3);
    Rumble alone = started();
    alone.hurt(3);
    check(with_hit.tick(f).large == alone.tick(play()).large, "a hit while hovering is as strong as alone");
}

void test_swim_dash() {
    Frame f = play();
    f.swim_dash = true;
    Rumble r = started();
    check(held(r, f, 100) == 100, "the Hydro-Pack's dash pushes every tick");
    const Motors left = r.tick(play());
    check(left.send && left.large == 0, "and stops the tick it ends");
    Settings off;
    off.grind = false;
    Rumble o = started(off);
    check(held(o, f, 10) == 0, "rails and swimming off: nothing");
}

void test_camera_shake() {
    Rumble r = started();
    Frame f = play();
    f.shake = rumble::kShakeFull;
    Motors m = r.tick(f);
    check(m.send && m.large == 255 && m.small == 1, "a full shake: both motors full");
    f.shake = rumble::kShakeFull / 2.0f;
    m = r.tick(f);
    check(m.large >= 125 && m.large <= 130 && m.small == 0, "half the shake, half the large motor");
    f.shake = 0.001f;
    m = r.tick(f);
    check(m.send && m.large == 0 && m.small == 0, "a trace of shake: stopped");
    Settings off;
    off.explosions = false;
    Rumble o = started(off);
    f.shake = 0.3f;
    check(!o.tick(f).send, "camera shake off: nothing");
}

// --- The game reader ---

struct FakeMemory {
    std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(0x200000);

    template <class T>
    T read(std::uint32_t address) const {
        T v;
        std::memcpy(&v, bytes.data() + address, sizeof v);
        return v;
    }

    template <class T>
    void write(std::uint32_t address, T v) {
        std::memcpy(bytes.data() + address, &v, sizeof v);
    }
};

void test_read_frame() {
    using namespace rac1_rumble;
    const Disc disc = disc_for(false);
    FakeMemory m;
    m.write<float>(disc.frame_time, 1.0f / 50.0f);
    m.write<std::int32_t>(disc.hero + kHealth, 4);
    m.write<std::int32_t>(disc.hero + kHeroState, 0x2);  // running

    Frame f = read_frame(m, disc, true, 0);
    check(f.playing && !f.dead && !f.grinding && !f.hovering && !f.swim_dash && !f.slam, "running: alive, nothing held");
    check(std::fabs(f.tick - 0.02f) < 1e-6f, "the game's tick");
    check(!read_frame(m, disc, false, 0).playing, "not in play");

    m.write<std::int32_t>(disc.hero + kHeroState, 0x28);
    check(read_frame(m, disc, true, 0).grinding, "state 0x28: on a rail");

    m.write<std::int32_t>(disc.hero + kHeroState, 0x81);
    check(read_frame(m, disc, true, 0).hovering, "state 0x81: the Thruster-Pack's hover");
    m.write<std::int32_t>(disc.hero + kHeroState, 0x8);
    check(read_frame(m, disc, true, 0).hovering, "state 0x8: the Heli-Pack's glide");
    m.write<std::int32_t>(disc.hero + kHeroState, 0x35);
    f = read_frame(m, disc, true, 0);
    check(f.swim_dash && !f.hovering, "state 0x35: the Hydro-Pack's dash");

    // The slam: frame 27 passed this tick (26.6 -> 27.4: delta 0.8) strikes; before it or a tick
    // later not.
    m.write<std::int32_t>(disc.hero + kHeroState, 0x14);
    m.write<float>(disc.hero + kAnimFrame, 27.4f);
    m.write<float>(disc.hero + kAnimFrameDelta, 0.8f);
    check(read_frame(m, disc, true, 0).slam, "the slam's animation passing frame 27: it strikes");
    m.write<float>(disc.hero + kAnimFrame, 28.2f);
    check(!read_frame(m, disc, true, 0).slam, "a tick later: no second strike");
    m.write<float>(disc.hero + kAnimFrame, 26.6f);
    check(!read_frame(m, disc, true, 0).slam, "before frame 27: not yet");
    m.write<std::int32_t>(disc.hero + kHeroState, 0x13);
    m.write<float>(disc.hero + kAnimFrame, 27.4f);
    check(!read_frame(m, disc, true, 0).slam, "frame 27 of another attack: no slam");

    m.write<std::int32_t>(disc.hero + kHeroState, 0x3D);
    check(read_frame(m, disc, true, 0).dead, "state 0x3D: dying");
    m.write<std::int32_t>(disc.hero + kHeroState, 0x0);
    m.write<std::int32_t>(disc.hero + kHealth, 0);
    check(read_frame(m, disc, true, 0).dead, "no health: dead");

    m.write<float>(disc.frame_time, 0.0f);
    check(std::fabs(read_frame(m, disc, true, 0).tick - 0.02f) < 1e-6f, "a nonsense tick: the disc's");
    check(std::fabs(tick_seconds(m, disc_for(true)) - 1.0f / 60.0f) < 1e-6f, "NTSC's is a 60th");
}

void test_camera_shake_reader() {
    using namespace rac1_rumble;
    FakeMemory m;
    const std::uint32_t shake = 0x166EE0;
    check(camera_shake(m, shake) == 0.0f, "no shake");
    check(camera_shake(m, 0) == 0.0f, "no level, no shake");
    // An explosion's, just written: amplitude 0.1, 20 frames, frames_max not yet set.
    m.write<float>(shake + kShakeAmplitude, 0.1f);
    m.write<std::int32_t>(shake + kShakeFrames, 20);
    check(std::fabs(camera_shake(m, shake) - 0.1f) < 1e-6f, "a new shake: its amplitude");
    m.write<std::int32_t>(shake + kShakeFramesMax, 20);
    m.write<std::int32_t>(shake + kShakeFrames, 10);
    check(std::fabs(camera_shake(m, shake) - 0.025f) < 1e-6f, "half way: a quarter (the game's square)");
    m.write<float>(shake + kShakeStride + kShakeAmplitude, 0.2f);
    m.write<std::int32_t>(shake + kShakeStride + kShakeFrames, 5);
    m.write<std::int32_t>(shake + kShakeStride + kShakeFramesMax, 5);
    check(std::fabs(camera_shake(m, shake) - 0.2f) < 1e-6f, "the stronger of the two channels");
    m.write<std::int32_t>(shake + kShakeFrames, 0);
    m.write<std::int32_t>(shake + kShakeStride + kShakeFrames, 0);
    check(camera_shake(m, shake) == 0.0f, "counted down: none");
}

}  // namespace

int main() {
    test_mixer_pulse_plays_its_length();
    test_mixer_fade_and_ease_decay_to_zero();
    test_mixer_delay_waits_and_averages_only_what_plays();
    test_mixer_averages_a_motor();
    test_mixer_full_bank_takes_the_shortest();
    test_mixer_permanent_plays_until_stopped();
    test_mixer_counted_wave_ends();
    test_every_pattern_ends();
    test_sends_every_tick_then_stops_once();
    test_heavier_hits_are_stronger();
    test_strength_scales();
    test_pause_stops_and_forgets();
    test_events_before_play_are_dropped();
    test_first_call();
    test_death_jolts_then_silence();
    test_toggles();
    test_weapon_classes();
    test_stream_holds_while_firing();
    test_continuous_does_not_halve_a_hit();
    test_grind();
    test_jumps();
    test_slam();
    test_hover();
    test_swim_dash();
    test_camera_shake();
    test_read_frame();
    test_camera_shake_reader();
    if (failures != 0) {
        std::printf("%d failed\n", failures);
        return 1;
    }
    std::printf("all passed\n");
    return 0;
}
