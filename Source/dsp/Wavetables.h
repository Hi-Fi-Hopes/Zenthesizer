#pragma once

// Engine B's four wavetables, built once at startup by additive synthesis.
// Each table has 8 frames (Position scans through them) and 8 band-limited
// copies per frame (one per octave), so high notes don't alias.

#include "Common.h"
#include <array>
#include <vector>

namespace zen
{
    struct Wavetables
    {
        static constexpr int size = 2048, mask = size - 1;
        static constexpr int frames = 8, levels = 8, maxHarm = 128;

        static const char* tableName (int t)
        {
            static const char* n[] = { "AIR", "GLASS", "VOX", "REED" };
            return n[std::min (3, std::max (0, t))];
        }

        static const Wavetables& get()
        {
            static const Wavetables w;
            return w;
        }

        const float* frame (int table, int fr, int level) const noexcept
        {
            return data.data() + (((size_t) table * frames + (size_t) fr) * levels + (size_t) level) * size;
        }

        // Harmonic amplitudes for one frame (index 1..maxHarm). Public so the UI can draw the tables.
        static void amplitudes (int table, float t, float* a)
        {
            for (int h = 0; h <= maxHarm; ++h) a[h] = 0.0f;
            switch (table)
            {
                case 0:   // AIR: pure sine opening into a soft, breathy saw
                {
                    const float p = 3.2f - 2.2f * t;
                    for (int h = 1; h <= maxHarm; ++h)
                        a[h] = (h == 1 ? 1.0f : std::pow ((float) h, -p) * (h % 2 == 0 ? 0.6f : 1.0f));
                    break;
                }
                case 1:   // GLASS: a bright partial cluster that rises with position, bell-like
                {
                    const float c = 2.0f + t * 22.0f;
                    for (int h = 1; h <= maxHarm; ++h)
                    {
                        const bool keep = (h % 2 == 1) || h == 2 || h == 4;
                        const float g = std::exp (-((float) h - c) * ((float) h - c) / (2.0f * 2.2f * 2.2f));
                        a[h] = (h == 1 ? 0.8f : 0.0f) + (keep ? g * 0.9f + 0.15f / (float) (h * h) : 0.0f);
                    }
                    break;
                }
                case 2:   // VOX: vowels a-e-i-o-u (formants for a ~220 Hz voice)
                {
                    static const float vf[5][3] = { { 800, 1150, 2900 }, { 400, 1600, 2700 }, { 270, 2140, 2950 },
                                                    { 450, 800, 2830 },  { 325, 700, 2530 } };
                    const float x = t * 4.0f;
                    const int i0 = std::min (3, (int) x);
                    const float fr = x - (float) i0;
                    for (int h = 1; h <= maxHarm; ++h)
                    {
                        const float hz = 220.0f * (float) h;
                        float env = 0.0f;
                        for (int k = 0; k < 3; ++k)
                        {
                            const float f = lerp (vf[i0][k], vf[i0 + 1][k], fr);
                            const float bw = 80.0f + 40.0f * (float) k;
                            const float g = (k == 0 ? 1.0f : k == 1 ? 0.6f : 0.35f);
                            env += g * std::exp (-(hz - f) * (hz - f) / (2.0f * bw * bw));
                        }
                        a[h] = (0.12f + env) * std::pow ((float) h, -0.8f);
                    }
                    break;
                }
                default:  // REED: square-ish to narrow reedy pulse
                {
                    const float w = 0.5f - 0.42f * t;
                    for (int h = 1; h <= maxHarm; ++h)
                        a[h] = std::abs (std::sin (pi * (float) h * w)) / (float) h * (1.0f - 0.25f * t * (h % 3 == 0 ? 1.0f : 0.0f));
                    break;
                }
            }
        }

    private:
        Wavetables()
        {
            data.assign ((size_t) numTables * frames * levels * size, 0.0f);
            std::vector<float> sine ((size_t) size);
            for (int i = 0; i < size; ++i) sine[(size_t) i] = std::sin (twoPi * (float) i / (float) size);

            float amp[maxHarm + 1];
            std::vector<float> tmp ((size_t) size);
            for (int tb = 0; tb < numTables; ++tb)
                for (int fr = 0; fr < frames; ++fr)
                {
                    amplitudes (tb, (float) fr / (float) (frames - 1), amp);
                    float norm = 1.0f;
                    for (int lv = 0; lv < levels; ++lv)
                    {
                        const int hmax = maxHarm >> lv;
                        std::fill (tmp.begin(), tmp.end(), 0.0f);
                        for (int h = 1; h <= hmax; ++h)
                        {
                            const float a = amp[h];
                            if (a < 1.0e-5f) continue;
                            for (int i = 0; i < size; ++i) tmp[(size_t) i] += a * sine[(size_t) ((i * h) & mask)];
                        }
                        if (lv == 0)
                        {
                            float peak = 1.0e-6f;
                            for (float v : tmp) peak = std::max (peak, std::abs (v));
                            norm = 1.0f / peak;
                        }
                        float* dst = data.data() + (((size_t) tb * frames + (size_t) fr) * levels + (size_t) lv) * size;
                        for (int i = 0; i < size; ++i) dst[i] = tmp[(size_t) i] * norm;
                    }
                }
        }

        std::vector<float> data;
    };

    // Reads Engine B: frame morph + linear interpolation, octave-matched band limit.
    struct WaveReader
    {
        static float read (const Wavetables& w, int table, float pos, float phase, float dt) noexcept
        {
            // Pick the band-limited level: harmonics allowed below ~0.45 * Nyquist-ish.
            const float allowed = 0.45f / std::max (1.0e-6f, dt);
            int lv = 0;
            while (lv < Wavetables::levels - 1 && (float) (Wavetables::maxHarm >> lv) > allowed) ++lv;

            const float fpos = clamp01 (pos) * (float) (Wavetables::frames - 1);
            const int f0 = std::min (Wavetables::frames - 2, (int) fpos);
            const float ff = fpos - (float) f0;

            const float x = phase * (float) Wavetables::size;
            const int i0 = (int) x;
            const float fx = x - (float) i0;
            const int a = i0 & Wavetables::mask, b = (i0 + 1) & Wavetables::mask;

            const float* t0 = w.frame (table, f0, lv);
            const float* t1 = w.frame (table, f0 + 1, lv);
            const float s0 = t0[a] + (t0[b] - t0[a]) * fx;
            const float s1 = t1[a] + (t1[b] - t1[a]) * fx;
            return s0 + (s1 - s0) * ff;
        }
    };
}
