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
    };

    //==========================================================================
    // Ready-made grooves. Rows are KICK, FRAME, WOOD, SHAKER, HAT, LOW, HIGH, BELL;
    // '.' = off, 'x' = on, 'X' = accent.
    struct Groove { const char* name; float bpm; float swing; const char* rows[numDrums]; };

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
        int type = 0;
        float t = 0.0f, vel = 1.0f, pan = 0.0f, gain = 1.0f;
        float tuneRatio = 1.0f, decayScale = 1.0f;
        double ph[6] {};
        float fm = 0.0f;
        Rng rng;
        SVF f1, f2;

        float render (float fs) noexcept
        {
            const float dt = 1.0f / fs;
            float y = 0.0f;
            auto expd = [this] (float tau) { return std::exp (-t / std::max (0.001f, tau)); };
            auto sine = [this, dt] (int i, float hz) { ph[i] += hz * dt; ph[i] -= std::floor (ph[i]); return std::sin (twoPi * (float) ph[i]); };
            float bp = 0.0f, hp = 0.0f;

            switch ((Drum) type)
            {
                case Drum::kick:
                {
                    const float f = 46.0f * tuneRatio + 120.0f * tuneRatio * expd (0.032f);
                    const float env = expd (0.32f * decayScale);
                    y = std::tanh (1.6f * sine (0, f) * env) * 0.95f;
                    if (t < 0.004f) y += rng.next() * 0.25f * (1.0f - t / 0.004f);
                    if (env < 0.0005f) active = false;
                    break;
                }
                case Drum::frame:
                {
                    const float tone = (sine (0, 182.0f * tuneRatio) * 0.6f + sine (1, 331.0f * tuneRatio) * 0.35f) * expd (0.09f * decayScale);
                    f1.process (rng.next(), bp, hp);
                    const float nz = bp * 2.2f * expd (0.16f * decayScale);
                    y = tone * 0.7f + nz * 0.8f;
                    if (t > 1.2f * decayScale + 0.1f) active = false;
                    break;
                }
                case Drum::wood:
                {
                    const float env = expd (0.045f * decayScale);
                    y = (sine (0, 880.0f * tuneRatio) * 0.7f + sine (1, 1392.0f * tuneRatio) * 0.35f) * env;
                    if (t < 0.002f) y += rng.next() * 0.4f;
                    if (env < 0.0005f) active = false;
                    break;
                }
                case Drum::shaker:
                {
                    f1.process (rng.next(), bp, hp);
                    const float att = std::min (1.0f, t / 0.009f);
                    const float env = att * expd (0.07f * decayScale);
                    y = hp * env * 0.9f;
                    if (t > 0.012f && env < 0.0005f) active = false;
                    break;
                }
                case Drum::hat:
                {
                    static const float r[6] = { 205.3f, 304.4f, 369.6f, 522.7f, 540.0f, 800.0f };
                    float m = 0.0f;
                    for (int i = 0; i < 6; ++i) { ph[i] += r[i] * 2.0f * tuneRatio * dt; ph[i] -= std::floor (ph[i]); m += ph[i] < 0.5 ? 1.0f : -1.0f; }
                    f1.process (m * 0.17f, bp, hp);
                    float b2, h2;
                    f2.process (bp, b2, h2);
                    const float env = expd (0.045f * decayScale);
                    y = h2 * env * 1.1f;
                    if (env < 0.0005f) active = false;
                    break;
                }
                case Drum::lowDrum:
                case Drum::highDrum:
                {
                    const float base = ((Drum) type == Drum::lowDrum ? 98.0f : 196.0f) * tuneRatio;
                    const float f = base * (1.0f + 0.45f * expd (0.04f));
                    const float env = expd ((type == (int) Drum::lowDrum ? 0.3f : 0.18f) * decayScale);
                    f1.process (rng.next(), bp, hp);
                    y = sine (0, f) * env * 0.9f + bp * expd (0.02f) * 0.5f;
                    if (env < 0.0005f) active = false;
                    break;
                }
                case Drum::bell:
                default:
                {
                    const float c = 880.0f * tuneRatio;
                    const float idx = 0.6f + 2.4f * expd (0.25f);
                    const float mod = sine (1, c * 2.76f) * idx;
                    ph[0] += c * dt; ph[0] -= std::floor (ph[0]);
                    const float env = expd (0.7f * decayScale);
                    y = std::sin (twoPi * (float) ph[0] + mod) * env * 0.45f;
                    if (env < 0.0005f) active = false;
                    break;
                }
            }
            t += dt;
            return y * vel * gain;
        }

        void start (int drumType, float velocity, float level, float tune, float decay, float fs, uint32_t seed)
        {
            active = true; type = drumType; t = 0.0f; vel = velocity; gain = level;
            tuneRatio = std::pow (2.0f, (clamp01 (tune) - 0.5f) * 2.0f);          // +- an octave
            decayScale = std::pow (4.0f, (clamp01 (decay) - 0.5f) * 2.0f);        // x0.25 .. x4
            for (auto& p : ph) p = 0.0;
            rng.s = seed | 1u;
            f1.clear(); f2.clear();
            switch ((Drum) type)
            {
                case Drum::frame:   f1.set (1800.0f * tuneRatio, 0.9f, fs); break;
                case Drum::shaker:  f1.set (5200.0f * tuneRatio, 0.7f, fs); break;
                case Drum::hat:     f1.set (9000.0f, 1.2f, fs); f2.set (7000.0f, 0.7f, fs); break;
                case Drum::lowDrum: case Drum::highDrum: f1.set (900.0f * tuneRatio, 1.0f, fs); break;
                default: break;
            }
            static const float pans[numDrums] = { 0.0f, 0.05f, -0.3f, 0.35f, 0.25f, -0.2f, 0.2f, -0.15f };
            pan = pans[std::min (numDrums - 1, std::max (0, type))];
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

            if (const int h = pendingHit.exchange (-1); h >= 0)
                nextVoice (h).start (h, 0.9f, d.level[(size_t) h], d.tune[(size_t) h], d.decay[(size_t) h], fs, ++seed * 2654435761u);

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
                        auto& voice = nextVoice (r);
                        voice.start (r, v == 2 ? 1.0f : 0.62f, d.level[(size_t) r], d.tune[(size_t) r], d.decay[(size_t) r], fs, ++seed * 2654435761u);
                        if (r == 0) ++kickCount;
                        ++hitCount;
                    }
                    ++t;
                }
                float l = 0.0f, rr = 0.0f;
                for (auto& v : voices)
                    if (v.active)
                    {
                        const float y = v.render (fs);
                        l += y * (1.0f - std::max (0.0f, v.pan));
                        rr += y * (1.0f + std::min (0.0f, v.pan));
                    }
                const float g = clamp01 (d.volume) * 0.8f;
                L[i] = l * g; R[i] = rr * g;
            }
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
