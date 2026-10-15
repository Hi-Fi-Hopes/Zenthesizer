#pragma once

// The BEAT section: eight synthesized, organic percussion voices and a 16-step pattern.
// Runs on its own clock (or Ableton's tempo and position), plays after the looper,
// and tells the looper where the bar lines are so loops stay locked to the beat.

#include "Common.h"
#include <array>
#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

namespace zen
{
    constexpr int numDrums = 8, numSteps = 16;

    enum class Drum : int { kick = 0, frame, wood, shaker, hat, lowDrum, highDrum, bell };

    inline const char* drumName (int d)
    {
        static const char* n[] = { "KICK", "FRAME", "WOOD", "SHAKER", "HAT", "LOW DRUM", "HIGH DRUM", "BELL" };
        return n[std::min (numDrums - 1, std::max (0, d))];
    }

    //==========================================================================
    // The sound library: any row can play any of these (or a captured synth patch).
    enum class Sound : int
    {
        kick = 0, frame, wood, shaker, hat, lowDrum, highDrum, bell,
        deepBoom, feltThump, subPulse, brush, clap, snap, rim, clave, logDrum, cajon,
        tambourine, cabasa, vinylTick, tabla, udu, conga, triangle, kalimba, glassDrop, raindrop, gong,
        count,
        patch = 100   // a captured synth patch (see DrumSample)
    };
    constexpr int numSounds = (int) Sound::count;

    inline const char* soundName (int s)
    {
        static const char* n[] = { "KICK", "FRAME DRUM", "WOOD BLOCK", "SHAKER", "HAT", "LOW DRUM", "HIGH DRUM", "BELL",
                                   "DEEP BOOM", "FELT THUMP", "SUB PULSE", "BRUSH", "CLAP", "SNAP", "RIM", "CLAVE", "LOG DRUM", "CAJON",
                                   "TAMBOURINE", "CABASA", "VINYL TICK", "TABLA", "UDU", "CONGA", "TRIANGLE", "KALIMBA", "GLASS DROP",
                                   "RAINDROP", "GONG" };
        if (s == (int) Sound::patch) return "PATCH";
        return n[std::min (numSounds - 1, std::max (0, s))];
    }

    // Menu groups: name, then the sounds in it
    struct SoundGroup { const char* name; std::vector<int> sounds; };
    inline const std::vector<SoundGroup>& soundGroups()
    {
        using S = Sound;
        static const std::vector<SoundGroup> g = {
            { "Kicks",          { (int) S::kick, (int) S::deepBoom, (int) S::feltThump, (int) S::subPulse } },
            { "Snares & claps", { (int) S::frame, (int) S::brush, (int) S::clap, (int) S::snap, (int) S::rim } },
            { "Woods",          { (int) S::wood, (int) S::clave, (int) S::logDrum, (int) S::cajon } },
            { "Shakers & hats", { (int) S::shaker, (int) S::hat, (int) S::tambourine, (int) S::cabasa, (int) S::vinylTick } },
            { "Hand drums",     { (int) S::lowDrum, (int) S::highDrum, (int) S::tabla, (int) S::udu, (int) S::conga } },
            { "Tonal",          { (int) S::triangle, (int) S::kalimba, (int) S::glassDrop, (int) S::raindrop, (int) S::gong, (int) S::bell } },
        };
        return g;
    }

    struct Kit { const char* name; std::array<int, 8> sounds; };
    inline const std::vector<Kit>& kits()
    {
        using S = Sound;
        static const std::vector<Kit> k = {
            { "Organic",     { (int) S::kick, (int) S::frame, (int) S::wood, (int) S::shaker, (int) S::hat, (int) S::lowDrum, (int) S::highDrum, (int) S::raindrop } },
            { "Hand Drums",  { (int) S::feltThump, (int) S::cajon, (int) S::clave, (int) S::shaker, (int) S::cabasa, (int) S::conga, (int) S::tabla, (int) S::udu } },
            { "Lo-Fi",       { (int) S::deepBoom, (int) S::brush, (int) S::rim, (int) S::vinylTick, (int) S::hat, (int) S::cajon, (int) S::snap, (int) S::kalimba } },
            { "Minimal",     { (int) S::subPulse, (int) S::snap, (int) S::clave, (int) S::cabasa, (int) S::vinylTick, (int) S::logDrum, (int) S::glassDrop, (int) S::triangle } },
            { "Temple",      { (int) S::deepBoom, (int) S::frame, (int) S::logDrum, (int) S::shaker, (int) S::triangle, (int) S::tabla, (int) S::kalimba, (int) S::gong } },
            { "Rain Garden", { (int) S::feltThump, (int) S::brush, (int) S::wood, (int) S::shaker, (int) S::tambourine, (int) S::udu, (int) S::raindrop, (int) S::glassDrop } },
            { "With Bell",   { (int) S::kick, (int) S::frame, (int) S::wood, (int) S::shaker, (int) S::hat, (int) S::lowDrum, (int) S::highDrum, (int) S::bell } },
        };
        return k;
    }

    // A captured synth patch: a stereo one-shot.
    struct DrumSample
    {
        std::vector<float> L, R;
    };

    struct DrumSettings
    {
        bool playing = false;
        float tempo = 90.0f;      // BPM when not following the host
        float swing = 0.0f;       // 0..1
        float volume = 0.6f;      // 0..1
        std::array<float, numDrums> level { 0.85f, 0.7f, 0.6f, 0.55f, 0.5f, 0.7f, 0.65f, 0.5f };
        std::array<float, numDrums> tune  { 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f };
        std::array<float, numDrums> decay { 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f };
        std::array<std::array<uint8_t, numSteps>, numDrums> steps {};   // 0 off, 1 on, 2 accent
        std::array<int, numDrums> sound { 0, 1, 2, 3, 4, 5, 6, (int) Sound::raindrop };   // the Organic kit
    };

    //==========================================================================
    // Ready-made grooves. Rows (in the Organic kit) are KICK, FRAME, WOOD, SHAKER, HAT, LOW, HIGH, RAINDROP;
    // every kit keeps those roles, so any groove works with any kit. '.' = off, 'x' = on, 'X' = accent.
    // folder: where the GROOVES menu files it ("" = the main list); kit: the kit it was written for.
    struct Groove { const char* name; float bpm; float swing; const char* rows[numDrums]; const char* folder = ""; const char* kit = ""; };

    inline const std::vector<Groove>& grooves()
    {
        static const std::vector<Groove> g = {
            { "Heartbeat", 70, 0.0f, { "X..x............", "................", "................", "........x.......", "................",
                                       "................", "................", "X...............", } },
            { "Forest Walk", 88, 0.15f, { "X......x..x.....", "....X.......X...", "..x.......x...x.", "x.x.x.x.x.x.x.x.", "................",
                                          "...........x....", "......x.........", "................", } },
            { "Trip Hop", 82, 0.2f, { "X.....x...X.....", "....X.......X...", "................", "..x...x...x...x.", "x.x.x.x.x.x.x.xx",
                                      "................", "................", "...............x", } },
            { "Hand Drum Circle", 96, 0.1f, { "X.......x.......", "................", "x..x..x...x..x..", "x.xxx.xxx.xxx.xx", "................",
                                              "X..x..X...x.....", "..x.x...x.x.x.xX", "................", } },
            { "Lo-Fi Shuffle", 78, 0.45f, { "X......x.X......", "....X.......X..x", "................", "................", "x.x.x.x.x.x.x.x.",
                                            "................", "................", "................", } },
            { "Afro Lilt", 104, 0.35f, { "X..x....X..x....", "......x.......x.", "x.xx.x.xx.xx.x.x", "x.x.x.x.x.x.x.x.", "................",
                                         "..x......x......", "....x..x....x..x", "................", } },
            { "Rainfall", 92, 0.1f, { "X.......X.......", "................", "................", "xxxXxxxXxxxXxxxX", "................",
                                      "................", "................", "x.....x.....x...", } },
            { "Temple", 72, 0.0f, { "X...............", "........X.......", "x...x...x...x...", "................", "................",
                                    "....x.......x...", "................", "X.......x.......", } },
            { "Soft Four", 118, 0.0f, { "X...X...X...X...", "....x.......x...", "................", "..x...x...x...x.", "..x...x...x...x.",
                                        "................", "................", "................", } },
            { "Half-Time Dream", 140, 0.0f, { "X.........x.....", "........X.......", "................", "x.x.x.x.x.x.x.x.", "................",
                                              "................", "..............x.", "X...............", } },
            { "Broken Branch", 100, 0.2f, { "X..x......X..x..", "....X.......X...", ".x....x.......x.", "................", "x.xxx.x.x.xxx.x.",
                                            "................", "........x.......", "................", } },
            { "Bossa Breeze", 112, 0.05f, { "X..x....X..x....", "................", "x..x..x...x..x..", "x.x.x.x.x.x.x.x.", "................",
                                            "................", "................", "................", } },
            { "Drone Pulse", 60, 0.0f, { "X.......x.......", "................", "................", "................", "................",
                                         "................", "................", "X.......x.......", } },
            { "Wood & Bell", 96, 0.25f, { "................", "................", "x..x..x.x..x..x.", "................", "................",
                                          "X.......X.......", "....x.......x...", "x.......x.....x.", } },
            { "Dusty Boom Bap", 86, 0.55f, { "X.....x...X.....", "....X.......X...", "...............x", "..x...x...x...x.", "x.x.x.x.x.x.x.x.", "................", "................", "......x........." }, "Lo-Fi", "Lo-Fi" },
            { "Rainy Day Beat", 74, 0.50f, { "X.........X..x..", "....X.......X...", "........x.......", "x.xxx.xxx.xxx.xx", "................", "................", "..........x.....", "X..............." }, "Lo-Fi", "Rain Garden" },
            { "Late Night Study", 82, 0.45f, { "X......xX.......", "....X.......X..x", "..x.......x.....", "................", "x.xxx.x.x.xxx.x.", "...........x....", "................", "................" }, "Lo-Fi", "Lo-Fi" },
            { "Tape Wobble", 90, 0.60f, { "X.x.......X.....", "....X.......X...", "................", "x.x.x.x.x.x.x.x.", "................", "......x.......x.", "...x............", "................" }, "Lo-Fi", "Lo-Fi" },
            { "Sleepy Shuffle", 70, 0.60f, { "X.........x.....", "........X.......", "..x...........x.", "x.x.x.x.x.x.x.x.", "................", "................", "................", "............x..." }, "Lo-Fi", "Lo-Fi" },
            { "Jazzhop Ghosts", 88, 0.50f, { "X......x..X.....", "....X..x....X.x.", "................", "................", "x.x.x.x.x.x.x.xx", "...x............", "...........x....", "................" }, "Lo-Fi", "Hand Drums" },
            { "Vinyl Sunday", 78, 0.40f, { "X.......X.x.....", "....X.......X...", "x...x...x...x...", "..x...x...x...x.", "................", "................", "................", "........x......." }, "Lo-Fi", "Lo-Fi" },
            { "Cozy Halftime", 72, 0.35f, { "X..x......x.....", "........X.......", "................", "x.x.x.x.x.x.x.x.", "....x.......x...", "..............x.", "................", "X..............." }, "Lo-Fi", "Minimal" },
            { "Distant Pulse", 64, 0.00f, { "X.......x.......", "................", "................", "........x.......", "................", "................", "................", "x.........x....." }, "Ambient", "Minimal" },
            { "Rain on Leaves", 72, 0.15f, { "................", "................", "...x.......x....", "x..x.x...x..x.x.", "..x.....x.....x.", "................", "................", ".....x.........x" }, "Ambient", "Rain Garden" },
            { "Temple Steps", 68, 0.00f, { "X...............", "................", "....x.......x...", "................", "................", "X.......x.......", "......x.......x.", "X..............." }, "Ambient", "Temple" },
            { "Floating", 76, 0.10f, { "................", "................", "................", "x.......x.......", "....x.......x...", "................", "................", "x.....x.....x..." }, "Ambient", "Rain Garden" },
            { "Slow Tide", 58, 0.00f, { "X...............", "........x.......", "................", "....x...........", "................", "................", "................", "X.......x......." }, "Ambient", "Minimal" },
            { "Breathing Room", 66, 0.20f, { "X.........x.....", "................", "......x.........", "x.x.....x.x.....", "................", "..............x.", "................", "....x..........." }, "Ambient", "Organic" },
            { "Glass Garden", 80, 0.10f, { "X.......X.......", "................", "................", "................", "..x...x...x...x.", "................", "................", "x..x..x...x..x.." }, "Ambient", "Minimal" },
            { "Empty", 90, 0.0f, { "................", "................", "................", "................", "................",
                                   "................", "................", "................", } },
        };
        return g;
    }

    inline void applyGroove (DrumSettings& d, const Groove& g)
    {
        for (int r = 0; r < numDrums; ++r)
            for (int s = 0; s < numSteps; ++s)
            {
                const char c = g.rows[r][s];
                d.steps[(size_t) r][(size_t) s] = c == 'X' ? 2 : (c == 'x' ? 1 : 0);
            }
        d.tempo = g.bpm;
        d.swing = g.swing;
    }

    // Pattern <-> text (for saving): 8 rows of 16 chars joined by '|'.
    inline std::string patternToString (const DrumSettings& d)
    {
        std::string s;
        for (int r = 0; r < numDrums; ++r)
        {
            for (int k = 0; k < numSteps; ++k) s += ".xX"[std::min<int> (2, d.steps[(size_t) r][(size_t) k])];
            if (r < numDrums - 1) s += '|';
        }
        return s;
    }
    inline bool patternFromString (DrumSettings& d, const std::string& s)
    {
        if ((int) s.size() != numDrums * numSteps + numDrums - 1) return false;
        for (int r = 0; r < numDrums; ++r)
            for (int k = 0; k < numSteps; ++k)
            {
                const char c = s[(size_t) (r * (numSteps + 1) + k)];
                d.steps[(size_t) r][(size_t) k] = c == 'X' ? 2 : (c == 'x' ? 1 : 0);
            }
        return true;
    }

    //==========================================================================
    // TPT state-variable filter with band and high outputs (for noise shaping).
    struct SVF
    {
        float g = 0.1f, k = 1.0f, a1 = 0, a2 = 0, a3 = 0, ic1 = 0, ic2 = 0;
        void set (float hz, float q, float fs) noexcept
        {
            g = std::tan (pi * std::min (hz, fs * 0.45f) / fs);
            k = 1.0f / q;
            a1 = 1.0f / (1.0f + g * (g + k)); a2 = g * a1; a3 = g * a2;
        }
        void process (float x, float& band, float& high) noexcept
        {
            const float v3 = x - ic2;
            const float v1 = a1 * ic1 + a2 * v3;
            const float v2 = ic2 + a2 * ic1 + a3 * v3;
            ic1 = 2.0f * v1 - ic1; ic2 = 2.0f * v2 - ic2;
            band = v1; high = x - k * v1 - v2;
        }
        void clear() noexcept { ic1 = ic2 = 0.0f; }
    };

    struct DrumVoice
    {
        bool active = false;
        int sound = 0;
        float t = 0.0f, vel = 1.0f, pan = 0.0f, gain = 1.0f;
        float tuneRatio = 1.0f, decayScale = 1.0f, decayKnob = 0.5f;
        double ph[6] {};
        Rng rng;
        SVF f1, f2;
        const DrumSample* sample = nullptr;
        double samplePos = 0.0;

        // one sample, stereo
        void render (float fs, float& outL, float& outR) noexcept
        {
            if (sound == (int) Sound::patch) { renderSample (fs, outL, outR); return; }

            const float dt = 1.0f / fs;
            float y = 0.0f;
            auto expd = [this] (float tau) { return std::exp (-t / std::max (0.0005f, tau)); };
            auto sine = [this, dt] (int i, float hz) { ph[i] += hz * dt; ph[i] -= std::floor (ph[i]); return std::sin (twoPi * (float) ph[i]); };
            auto done = [this] (float env, float minT = 0.0f) { if (t > minT && env < 0.0005f) active = false; };
            float bp = 0.0f, hp = 0.0f;
            const float tr = tuneRatio, ds = decayScale;

            switch ((Sound) sound)
            {
                case Sound::kick:
                {
                    const float env = expd (0.32f * ds);
                    y = std::tanh (1.6f * sine (0, 46.0f * tr + 120.0f * tr * expd (0.032f)) * env) * 0.95f;
                    if (t < 0.004f) y += rng.next() * 0.25f * (1.0f - t / 0.004f);
                    done (env); break;
                }
                case Sound::deepBoom:
                {
                    const float env = expd (0.7f * ds);
                    y = std::tanh (2.0f * sine (0, 38.0f * tr + 70.0f * tr * expd (0.06f)) * env) * 0.9f;
                    done (env); break;
                }
                case Sound::feltThump:
                {
                    const float env = expd (0.18f * ds);
                    f1.process (sine (0, 62.0f * tr + 40.0f * tr * expd (0.02f)), bp, hp);
                    y = (sine (1, 62.0f * tr) * 0.7f + bp * 0.6f) * env;
                    done (env); break;
                }
                case Sound::subPulse:
                {
                    const float env = std::min (1.0f, t / 0.005f) * expd (0.45f * ds);
                    y = sine (0, 45.0f * tr) * env * 0.95f;
                    done (env, 0.01f); break;
                }
                case Sound::frame:
                {
                    const float tone = (sine (0, 182.0f * tr) * 0.6f + sine (1, 331.0f * tr) * 0.35f) * expd (0.09f * ds);
                    f1.process (rng.next(), bp, hp);
                    y = tone * 0.7f + bp * 2.2f * expd (0.16f * ds) * 0.8f;
                    if (t > 1.2f * ds + 0.1f) active = false;
                    break;
                }
                case Sound::brush:
                {
                    f1.process (rng.next(), bp, hp);
                    const float env = std::min (1.0f, t / 0.015f) * expd (0.18f * ds);
                    y = bp * 1.6f * env + sine (0, 200.0f * tr) * expd (0.05f) * 0.2f;
                    done (env, 0.02f); break;
                }
                case Sound::clap:
                {
                    f1.process (rng.next(), bp, hp);
                    float env = 0.0f;
                    for (int k = 0; k < 3; ++k) { const float tk = t - 0.011f * (float) k; if (tk >= 0.0f) env = std::max (env, std::exp (-tk / 0.006f)); }
                    if (t > 0.022f) env = std::max (env, 0.6f * std::exp (-(t - 0.022f) / (0.12f * ds)));
                    y = bp * 2.4f * env;
                    done (env, 0.03f); break;
                }
                case Sound::snap:
                {
                    f1.process (rng.next(), bp, hp);
                    const float env = expd (0.022f * ds);
                    y = bp * 2.6f * env + sine (0, 1800.0f * tr) * expd (0.008f) * 0.4f;
                    done (env); break;
                }
                case Sound::rim:
                {
                    f1.process (rng.next(), bp, hp);
                    const float env = expd (0.02f * ds);
                    y = sine (0, 1650.0f * tr) * expd (0.012f * ds) * 0.6f + sine (1, 450.0f * tr) * env * 0.5f + bp * expd (0.008f) * 1.2f;
                    done (env); break;
                }
                case Sound::wood:
                {
                    const float env = expd (0.045f * ds);
                    y = (sine (0, 880.0f * tr) * 0.7f + sine (1, 1392.0f * tr) * 0.35f) * env;
                    if (t < 0.002f) y += rng.next() * 0.4f;
                    done (env); break;
                }
                case Sound::clave:
                {
                    const float env = expd (0.06f * ds);
                    y = (sine (0, 2500.0f * tr) * 0.75f + sine (1, 4120.0f * tr) * 0.2f * expd (0.01f)) * env * 0.8f;
                    done (env); break;
                }
                case Sound::logDrum:
                {
                    const float env = expd (0.25f * ds);
                    const float f = 330.0f * tr * (1.0f + 0.08f * expd (0.02f));
                    y = (sine (0, f) * 0.8f + sine (1, f * 2.3f) * 0.3f * expd (0.05f * ds)) * env;
                    done (env); break;
                }
                case Sound::cajon:
                {
                    f1.process (rng.next(), bp, hp);
                    const float env = expd (0.08f * ds);
                    y = sine (0, 120.0f * tr * (1.0f + 0.3f * expd (0.01f))) * env * 0.9f + bp * 1.5f * expd (0.05f * ds);
                    done (env); break;
                }
                case Sound::shaker:
                {
                    f1.process (rng.next(), bp, hp);
                    const float env = std::min (1.0f, t / 0.009f) * expd (0.07f * ds);
                    y = hp * env * 0.9f;
                    done (env, 0.012f); break;
                }
                case Sound::cabasa:
                {
                    f1.process (rng.next(), bp, hp);
                    const float env = std::min (1.0f, t / 0.012f) * expd (0.05f * ds);
                    y = hp * env * 1.1f;
                    done (env, 0.015f); break;
                }
                case Sound::vinylTick:
                {
                    f1.process (rng.next(), bp, hp);
                    const float env = expd (0.0015f * ds);
                    y = bp * env * 2.0f;
                    done (env); break;
                }
                case Sound::hat:
                case Sound::tambourine:
                {
                    static const float r[6] = { 205.3f, 304.4f, 369.6f, 522.7f, 540.0f, 800.0f };
                    const bool tamb = (Sound) sound == Sound::tambourine;
                    float m = 0.0f;
                    for (int i = 0; i < 6; ++i) { ph[i] += r[i] * (tamb ? 5.2f : 2.0f) * tr * dt; ph[i] -= std::floor (ph[i]); m += ph[i] < 0.5 ? 1.0f : -1.0f; }
                    f1.process (m * 0.17f + (tamb ? rng.next() * 0.2f : 0.0f), bp, hp);
                    float b2, h2;
                    f2.process (bp, b2, h2);
                    const float env = expd ((tamb ? 0.15f : 0.045f) * ds) * (tamb ? (0.75f + 0.25f * std::sin (t * 180.0f)) : 1.0f);
                    y = h2 * env * (tamb ? 0.9f : 1.1f);
                    done (env); break;
                }
                case Sound::lowDrum:
                case Sound::highDrum:
                {
                    const float base = ((Sound) sound == Sound::lowDrum ? 98.0f : 196.0f) * tr;
                    const float env = expd (((Sound) sound == Sound::lowDrum ? 0.3f : 0.18f) * ds);
                    f1.process (rng.next(), bp, hp);
                    y = sine (0, base * (1.0f + 0.45f * expd (0.04f))) * env * 0.9f + bp * expd (0.02f) * 0.5f;
                    done (env); break;
                }
                case Sound::tabla:
                {
                    const float env = expd (0.4f * ds);
                    const float f = 420.0f * tr;
                    y = (sine (0, f) * 0.6f + sine (1, f * 2.0f) * 0.3f * expd (0.15f) + sine (2, f * 3.0f) * 0.15f * expd (0.08f)) * env;
                    if (t < 0.006f) y += rng.next() * 0.3f * (1.0f - t / 0.006f);
                    done (env); break;
                }
                case Sound::udu:
                {
                    const float env = expd (0.12f * ds);
                    y = sine (0, 400.0f * tr * (1.0f - 0.5f * expd (0.04f))) * env * 0.9f;   // the pitch rises: a water drop
                    done (env); break;
                }
                case Sound::raindrop:
                {
                    const float env = expd (0.05f * ds);
                    y = sine (0, 900.0f * tr * (1.0f - 0.6f * expd (0.015f))) * env * 0.75f;
                    done (env); break;
                }
                case Sound::conga:
                {
                    f1.process (rng.next(), bp, hp);
                    const float env = expd (0.22f * ds);
                    y = sine (0, 200.0f * tr * (1.0f + 0.25f * expd (0.015f))) * env * 0.85f + bp * expd (0.01f) * 0.6f;
                    done (env); break;
                }
                case Sound::triangle:
                {
                    const float env = expd (1.2f * ds);
                    y = (sine (0, 4200.0f * tr) * 0.5f + sine (1, 6100.0f * tr) * 0.25f) * env * 0.45f;
                    done (env); break;
                }
                case Sound::kalimba:
                {
                    const float env = expd (0.6f * ds);
                    y = (sine (0, 660.0f * tr) * 0.8f + sine (1, 660.0f * tr * 6.3f) * 0.25f * expd (0.02f)) * env * 0.7f;
                    if (t < 0.002f) y += rng.next() * 0.2f;
                    done (env); break;
                }
                case Sound::glassDrop:
                {
                    const float env = expd (0.35f * ds);
                    const float f = 1400.0f * tr * (1.0f + 0.04f * expd (0.03f));
                    y = (sine (0, f) * 0.6f + sine (1, f * 2.76f) * 0.25f * expd (0.08f)) * env * 0.6f;
                    done (env); break;
                }
                case Sound::gong:
                {
                    const float c = 110.0f * tr;
                    const float idx = 0.8f + 1.2f * std::min (1.0f, t / 0.3f) * expd (1.5f);
                    const float mod = sine (1, c * 1.414f) * idx;
                    ph[0] += c * dt; ph[0] -= std::floor (ph[0]);
                    const float env = std::min (1.0f, t / 0.06f) * expd (2.5f * ds);
                    y = std::sin (twoPi * (float) ph[0] + mod) * env * 0.55f;
                    done (env, 0.1f); break;
                }
                case Sound::bell:
                default:
                {
                    const float c = 880.0f * tr;
                    const float mod = sine (1, c * 2.76f) * (0.6f + 2.4f * expd (0.25f));
                    ph[0] += c * dt; ph[0] -= std::floor (ph[0]);
                    const float env = expd (0.7f * ds);
                    y = std::sin (twoPi * (float) ph[0] + mod) * env * 0.45f;
                    done (env); break;
                }
            }
            t += dt;
            y *= vel * gain;
            outL += y * (1.0f - std::max (0.0f, pan));
            outR += y * (1.0f + std::min (0.0f, pan));
        }

        void renderSample (float fs, float& outL, float& outR) noexcept
        {
            if (sample == nullptr || sample->L.empty()) { active = false; return; }
            const int i = (int) samplePos;
            const int len = (int) sample->L.size();
            if (i >= len - 1) { active = false; return; }
            // DECAY below the middle shortens the sound with a fade
            float env = 1.0f;
            if (decayKnob < 0.5f)
            {
                const float keep = 0.05f + 1.9f * decayKnob;                // fraction of the length kept
                const float x = (float) samplePos / ((float) len * keep);
                env = x < 0.7f ? 1.0f : std::max (0.0f, 1.0f - (x - 0.7f) / 0.3f);
                if (env <= 0.0f) { active = false; return; }
            }
            const float g = vel * gain * env;
            outL += sample->L[(size_t) i] * g;
            outR += sample->R[(size_t) i] * g;
            samplePos += 1.0;
            t += 1.0f / fs;
        }

        void start (int snd, float velocity, float level, float tune, float decay, float fs, uint32_t seed, float rowPan,
                    const DrumSample* smp)
        {
            active = true; sound = snd; t = 0.0f; vel = velocity; gain = level; pan = rowPan;
            tuneRatio = std::pow (2.0f, (clamp01 (tune) - 0.5f) * 2.0f);          // +- an octave
            decayScale = std::pow (4.0f, (clamp01 (decay) - 0.5f) * 2.0f);        // x0.25 .. x4
            decayKnob = clamp01 (decay);
            for (auto& p : ph) p = 0.0;
            rng.s = seed | 1u;
            f1.clear(); f2.clear();
            sample = smp; samplePos = 0.0;
            if (snd == (int) Sound::patch && smp == nullptr) { active = false; return; }
            switch ((Sound) snd)
            {
                case Sound::frame:      f1.set (1800.0f * tuneRatio, 0.9f, fs); break;
                case Sound::shaker:     f1.set (5200.0f * tuneRatio, 0.7f, fs); break;
                case Sound::cabasa:     f1.set (7500.0f * tuneRatio, 0.7f, fs); break;
                case Sound::hat:        f1.set (9000.0f, 1.2f, fs); f2.set (7000.0f, 0.7f, fs); break;
                case Sound::tambourine: f1.set (10000.0f, 1.0f, fs); f2.set (6500.0f, 0.7f, fs); break;
                case Sound::lowDrum: case Sound::highDrum: f1.set (900.0f * tuneRatio, 1.0f, fs); break;
                case Sound::conga:      f1.set (1500.0f * tuneRatio, 1.0f, fs); break;
                case Sound::cajon:      f1.set (900.0f * tuneRatio, 0.8f, fs); break;
                case Sound::brush:      f1.set (3500.0f * tuneRatio, 0.6f, fs); break;
                case Sound::clap:       f1.set (1200.0f * tuneRatio, 1.2f, fs); break;
                case Sound::snap:       f1.set (2500.0f * tuneRatio, 2.0f, fs); break;
                case Sound::rim:        f1.set (4000.0f * tuneRatio, 1.5f, fs); break;
                case Sound::vinylTick:  f1.set (3000.0f * tuneRatio, 0.8f, fs); break;
                case Sound::feltThump:  f1.set (180.0f * tuneRatio, 0.7f, fs); break;
                default: break;
            }
        }
    };

    //==========================================================================
    class DrumMachine
    {
    public:
        static constexpr int maxBars = 8;

        void prepare (float sampleRate)
        {
            fs = sampleRate;
            for (auto& v : voices) v.active = false;
            wasPlaying = false; beat = 0.0;
        }

        // Renders n samples of drums into L/R (overwrites), and lists where bar lines fall in the block.
        // hostPpq >= 0 locks the pattern to the host's position (while the host is playing).
        void process (const DrumSettings& d, float bpm, double hostPpq, float* L, float* R, int n,
                      int* barOffsets, int& numBarOffsets) noexcept
        {
            numBarOffsets = 0;
            const float tempo = std::max (20.0f, bpm);
            const double beatsPerSample = (double) tempo / 60.0 / (double) fs;

            struct Trig { int offset; int step; };
            Trig trigs[64]; int numTrigs = 0;

            if (d.playing)
            {
                if (hostPpq >= 0.0) beat = hostPpq;
                else if (! wasPlaying) beat = -1.0e-9;    // start right on the downbeat
                const double b0 = beat, b1 = beat + beatsPerSample * n;
                const double stepLen = 0.25;
                const double swingOff = clamp01 (d.swing) * 0.33 * stepLen;

                for (long long k = (long long) std::floor (b0 / stepLen) - 1; k <= (long long) std::ceil (b1 / stepLen) && numTrigs < 64; ++k)
                {
                    const double tk = (double) k * stepLen + ((k & 1) ? swingOff : 0.0);
                    if (tk >= b0 && tk < b1)
                        trigs[numTrigs++] = { std::min (n - 1, (int) ((tk - b0) / beatsPerSample)), (int) (((k % numSteps) + numSteps) % numSteps) };
                }
                for (long long m = (long long) std::ceil (b0 / 4.0); (double) m * 4.0 < b1 && numBarOffsets < maxBars; ++m)
                    barOffsets[numBarOffsets++] = std::min (n - 1, std::max (0, (int) (((double) m * 4.0 - b0) / beatsPerSample)));

                beat = b1;
                currentStep = (int) (((long long) std::floor (std::max (0.0, b1) / stepLen)) % numSteps);
                beatPos = (float) std::fmod (std::max (0.0, b1), 4.0) / 4.0f;
            }
            else currentStep = -1;
            wasPlaying = d.playing;

            if (const int h = pendingHit.exchange (-1); h >= 0) startRow (d, h, 0.9f);

            int t = 0;
            for (int i = 0; i < n; ++i)
            {
                while (t < numTrigs && trigs[t].offset == i)
                {
                    const int step = trigs[t].step;
                    for (int r = 0; r < numDrums; ++r)
                    {
                        const int v = d.steps[(size_t) r][(size_t) step];
                        if (v == 0) continue;
                        startRow (d, r, v == 2 ? 1.0f : 0.62f);
                        if (r == 0) ++kickCount;
                        ++hitCount;
                    }
                    ++t;
                }
                float l = 0.0f, rr = 0.0f;
                for (auto& v : voices)
                    if (v.active) v.render (fs, l, rr);
                const float g = clamp01 (d.volume) * 0.8f;
                L[i] = l * g; R[i] = rr * g;
            }
        }

        // A captured patch for a row (owner keeps it alive a while after replacing it)
        void setSample (int row, const DrumSample* smp) noexcept
        {
            if (row >= 0 && row < numDrums) samples[(size_t) row].store (smp);
        }

        // UI: play one drum now (to hear it while editing)
        void requestHit (int drum) noexcept { if (drum >= 0 && drum < numDrums) pendingHit.store (drum); }

        int currentStep = -1;
        float beatPos = 0.0f;
        int kickCount = 0, hitCount = 0;

    private:
        float fs = 48000.0f;
        double beat = 0.0;
        bool wasPlaying = false;
        uint32_t seed = 1;
        std::atomic<int> pendingHit { -1 };
        std::array<std::atomic<const DrumSample*>, numDrums> samples {};

        void startRow (const DrumSettings& d, int r, float velocity) noexcept
        {
            static const float pans[numDrums] = { 0.0f, 0.05f, -0.3f, 0.35f, 0.25f, -0.2f, 0.2f, -0.15f };
            const int snd = d.sound[(size_t) r];
            const DrumSample* smp = snd == (int) Sound::patch ? samples[(size_t) r].load() : nullptr;
            nextVoice (r).start (snd, velocity, d.level[(size_t) r], d.tune[(size_t) r], d.decay[(size_t) r], fs,
                                 ++seed * 2654435761u, pans[r], smp);
        }
        std::array<DrumVoice, numDrums * 2> voices {};
        std::array<int, numDrums> rr {};

        DrumVoice& nextVoice (int drum) noexcept
        {
            // two voices per drum, alternating, so a retrigger doesn't cut the last hit off with a click
            const int which = rr[(size_t) drum] ^= 1;
            return voices[(size_t) (drum * 2 + which)];
        }
    };
}
