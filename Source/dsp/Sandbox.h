#pragma once

// Tools for the wavetable sandbox (no JUCE): starting shapes, smoothing, harmonic editing,
// and turning imported audio into keyframes.

#include "Common.h"
#include "Wavetables.h"
#include <vector>

namespace zen::sandbox
{
    using Cycle = std::vector<float>;   // tableSize samples

    inline Cycle blank() { return Cycle ((size_t) tableSize, 0.0f); }

    enum class Shape { sine = 0, triangle, saw, square, pulse, halfSine, organ, hollow, bell, noise, count };

    inline const char* shapeName (Shape s)
    {
        static const char* n[] = { "Sine", "Triangle", "Saw", "Square", "Narrow pulse", "Half sine", "Organ",
                                   "Hollow", "Bell", "Smooth noise" };
        return n[(int) s];
    }

    inline Cycle make (Shape s, uint32_t seed = 1)
    {
        Cycle c = blank();
        const int N = tableSize;
        for (int n = 0; n < N; ++n)
        {
            const float p = (float) n / (float) N;
            float v = 0.0f;
            switch (s)
            {
                case Shape::sine:     v = std::sin (twoPi * p); break;
                case Shape::triangle: v = 1.0f - 4.0f * std::abs (std::fmod (p + 0.25f, 1.0f) - 0.5f); break;
                case Shape::saw:      v = 1.0f - 2.0f * p; break;
                case Shape::square:   v = p < 0.5f ? 1.0f : -1.0f; break;
                case Shape::pulse:    v = p < 0.12f ? 1.0f : -0.14f; break;
                case Shape::halfSine: v = p < 0.5f ? std::sin (twoPi * p) * 2.0f - 0.64f : -0.64f; break;
                case Shape::organ:    v = std::sin (twoPi * p) + 0.6f * std::sin (2 * twoPi * p) + 0.45f * std::sin (3 * twoPi * p)
                                        + 0.3f * std::sin (4 * twoPi * p) + 0.25f * std::sin (6 * twoPi * p) + 0.2f * std::sin (8 * twoPi * p); break;
                case Shape::hollow:   for (int h = 1; h < 24; h += 2) v += std::sin (h * twoPi * p) / (float) (h * h) * (h % 4 == 1 ? 1.0f : -1.0f) * (float) h * 0.6f; break;
                case Shape::bell:     v = std::sin (twoPi * p) + 0.5f * std::sin (5 * twoPi * p)
                                        + 0.35f * std::sin (9 * twoPi * p) + 0.2f * std::sin (14 * twoPi * p); break;
                default: break;
            }
            c[(size_t) n] = v;
        }
        if (s == Shape::noise)
        {
            // random harmonics with a soft spectral tilt: smooth, but different every time
            Rng r; r.s = seed * 2654435761u | 1u;
            std::vector<float> re (UserTable::maxHarm + 1, 0.0f), im (UserTable::maxHarm + 1, 0.0f);
            for (int h = 1; h <= 48; ++h)
            {
                const float a = std::pow ((float) h, -1.1f);
                re[(size_t) h] = a * r.next(); im[(size_t) h] = a * r.next();
            }
            UserTable::synth (re.data(), im.data(), 48, c.data());
        }
        float peak = 1.0e-6f;
        for (float v : c) peak = std::max (peak, std::abs (v));
        for (auto& v : c) v /= peak;
        return c;
    }

    // Copies one frame of a built-in table (level 0 = full bandwidth).
    inline Cycle fromBuiltIn (int table, int frame)
    {
        const auto v = Wavetables::get().view (table);
        const float* f = v.frame (std::min (v.frames - 1, std::max (0, frame)), 0);
        return Cycle (f, f + tableSize);
    }

    // Circular smoothing: a few passes of a small box filter.
    inline void smooth (Cycle& c, int width = 9, int passes = 2)
    {
        const int N = tableSize, half = width / 2;
        Cycle tmp ((size_t) N);
        for (int p = 0; p < passes; ++p)
        {
            for (int n = 0; n < N; ++n)
            {
                float acc = 0.0f;
                for (int k = -half; k <= half; ++k) acc += c[(size_t) ((n + k + N) % N)];
                tmp[(size_t) n] = acc / (float) (2 * half + 1);
            }
            c.swap (tmp);
        }
    }

    // Draws a straight line between two points of a stroke (indices may wrap the wrong way: no wrap).
    inline void drawLine (Cycle& c, float x0, float y0, float x1, float y1)
    {
        const int N = tableSize;
        int i0 = (int) std::lround (x0 * (float) (N - 1)), i1 = (int) std::lround (x1 * (float) (N - 1));
        i0 = std::min (N - 1, std::max (0, i0)); i1 = std::min (N - 1, std::max (0, i1));
        if (i0 == i1) { c[(size_t) i0] = clampf (y1, -1.0f, 1.0f); return; }
        const int step = i1 > i0 ? 1 : -1;
        for (int i = i0; i != i1 + step; i += step)
        {
            const float t = (float) (i - i0) / (float) (i1 - i0);
            c[(size_t) i] = clampf (y0 + (y1 - y0) * t, -1.0f, 1.0f);
        }
    }

    // Harmonic view of a cycle: magnitudes and phases for h = 1..maxHarm.
    struct Harmonics
    {
        std::vector<float> mag, phase;   // index 0 unused

        static Harmonics of (const Cycle& c)
        {
            Harmonics h;
            std::vector<float> re (UserTable::maxHarm + 1, 0.0f), im (UserTable::maxHarm + 1, 0.0f);
            UserTable::analyse (c.data(), re.data(), im.data());
            h.mag.assign (UserTable::maxHarm + 1, 0.0f);
            h.phase.assign (UserTable::maxHarm + 1, 0.0f);
            for (int k = 1; k <= UserTable::maxHarm; ++k)
            {
                h.mag[(size_t) k] = std::sqrt (re[(size_t) k] * re[(size_t) k] + im[(size_t) k] * im[(size_t) k]);
                h.phase[(size_t) k] = std::atan2 (re[(size_t) k], im[(size_t) k]);   // sine phase
            }
            return h;
        }

        Cycle toCycle() const
        {
            std::vector<float> re (UserTable::maxHarm + 1, 0.0f), im (UserTable::maxHarm + 1, 0.0f);
            for (int k = 1; k <= UserTable::maxHarm; ++k)
            {
                re[(size_t) k] = mag[(size_t) k] * std::sin (phase[(size_t) k]);
                im[(size_t) k] = mag[(size_t) k] * std::cos (phase[(size_t) k]);
            }
            Cycle c = blank();
            UserTable::synth (re.data(), im.data(), UserTable::maxHarm, c.data());
            return c;
        }

        float maxOf (int upTo) const
        {
            float m = 1.0e-6f;
            for (int k = 1; k <= upTo && k < (int) mag.size(); ++k) m = std::max (m, mag[(size_t) k]);
            return m;
        }
    };

    inline void normalise (Cycle& c)
    {
        float mean = 0.0f;
        for (float v : c) mean += v;
        mean /= (float) tableSize;
        float peak = 1.0e-6f;
        for (auto& v : c) { v -= mean; peak = std::max (peak, std::abs (v)); }
        for (auto& v : c) v /= peak;
    }

    // Resample any length of audio to one cycle (linear interpolation, treated as periodic).
    inline Cycle resampleCycle (const float* x, int len)
    {
        Cycle c = blank();
        for (int n = 0; n < tableSize; ++n)
        {
            const float pos = (float) n * (float) len / (float) tableSize;
            const int i = (int) pos;
            const float f = pos - (float) i;
            c[(size_t) n] = x[i % len] + (x[(i + 1) % len] - x[i % len]) * f;
        }
        return c;
    }

    // Turns imported audio (mono) into 1..8 keyframes.
    //  - exact multiples of 2048 samples: a wavetable file (Serum-style), up to 8 frames picked evenly
    //  - short files (up to 4096 samples): one single cycle, stretched to fit
    //  - anything longer: 8 slices across the file, each faded at its loop point so it doesn't click
    inline std::vector<Cycle> keyframesFromAudio (const std::vector<float>& audio, int frameSize = tableSize)
    {
        std::vector<Cycle> out;
        const int len = (int) audio.size();
        if (len < 8) return out;

        if (frameSize > 0 && len % frameSize == 0)
        {
            const int total = len / frameSize;
            const int n = std::min (maxKeyframes, total);
            for (int k = 0; k < n; ++k)
            {
                const int src = n == 1 ? 0 : (int) std::lround ((double) k * (double) (total - 1) / (double) (n - 1));
                out.push_back (resampleCycle (audio.data() + (size_t) src * (size_t) frameSize, frameSize));
            }
        }
        else if (len <= 4096)
        {
            out.push_back (resampleCycle (audio.data(), len));
        }
        else
        {
            const int n = maxKeyframes;
            const int slice = std::min (tableSize, len / n);
            for (int k = 0; k < n; ++k)
            {
                const int start = (int) ((double) k * (double) (len - slice) / (double) (n - 1));
                std::vector<float> seg (audio.begin() + start, audio.begin() + start + slice);
                const int fade = std::max (1, slice / 8);
                for (int i = 0; i < fade; ++i)   // crossfade the tail into the head so the loop is seamless
                {
                    const float t = (float) i / (float) fade;
                    seg[(size_t) i] = seg[(size_t) i] * t + seg[(size_t) (slice - fade + i)] * (1.0f - t);
                }
                seg.resize ((size_t) (slice - fade));
                out.push_back (resampleCycle (seg.data(), (int) seg.size()));
            }
        }
        for (auto& c : out) normalise (c);
        return out;
    }
}
