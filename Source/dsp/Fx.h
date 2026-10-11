#pragma once

// The space around the voices: chorus -> ping-pong delay -> shimmer reverb.

#include "Common.h"
#include "Maps.h"
#include <array>

namespace zen
{
    struct Chorus
    {
        void prepare (float sampleRate)
        {
            fs = sampleRate;
            dl.allocate ((int) (0.03f * fs)); dr.allocate ((int) (0.03f * fs));
            phase = 0.0f;
        }
        void clear() { dl.clear(); dr.clear(); }

        void process (float* L, float* R, int n, float mix, float depth, float rate) noexcept
        {
            if (mix < 0.0005f && lastMix < 0.0005f) { for (int i = 0; i < n; ++i) { dl.push (L[i]); dr.push (R[i]); } return; }
            const float inc = (0.08f * std::pow (40.0f, clamp01 (rate))) / fs;   // 0.08 .. 3.2 Hz
            const float base = 0.007f * fs, swing = (0.0015f + 0.005f * clamp01 (depth)) * fs;
            for (int i = 0; i < n; ++i)
            {
                const float m = lastMix + (mix - lastMix) * ((float) i / (float) n);
                dl.push (L[i]); dr.push (R[i]);
                const float wl = dl.readHermite (base + swing * (0.5f + 0.5f * std::sin (twoPi * phase)));
                const float wr = dr.readHermite (base + swing * (0.5f + 0.5f * std::sin (twoPi * (phase + 0.25f))));
                phase += inc; if (phase >= 1.0f) phase -= 1.0f;
                const float norm = 1.0f / (1.0f + 0.35f * m);
                L[i] = (L[i] + m * wl) * norm;
                R[i] = (R[i] + m * wr) * norm;
            }
            lastMix = mix;
        }

        DelayBuffer dl, dr;
        float fs = 48000.0f, phase = 0.0f, lastMix = 0.0f;
    };

    //==========================================================================
    struct PingPong
    {
        void prepare (float sampleRate)
        {
            fs = sampleRate;
            bl.allocate ((int) (3.2f * fs)); br.allocate ((int) (3.2f * fs));
            time.setTime (0.12f * fs);
            time.snap (0.3f * fs);
            lpL.clear(); lpR.clear();
            wow.prepare (fs, 0.6f, 77u);
        }
        void clear() { bl.clear(); br.clear(); lpL.clear(); lpR.clear(); }

        void process (float* L, float* R, int n, float t, int sync, float bpm, float feedback, float tone, float mix) noexcept
        {
            float samples;
            const float beats = delaySyncBeats (sync);
            if (beats > 0.0f) samples = beats * 60.0f / (bpm > 1.0f ? bpm : 110.0f) * fs;
            else samples = delayMs (t) * 0.001f * fs;
            time.target = clampf (samples, 16.0f, 3.0f * fs);

            const float fb = clampf (feedback, 0.0f, 1.0f) * 0.92f;
            lpL.setCutoff (500.0f * std::pow (32.0f, clamp01 (tone)), fs);
            lpR.G = lpL.G;

            for (int i = 0; i < n; ++i)
            {
                const float m = lastMix + (mix - lastMix) * ((float) i / (float) n);
                const float d = time.next() * (1.0f + 0.0012f * wow.next());
                const float yl = bl.readHermite (d), yr = br.readHermite (d);
                const float in = 0.5f * (L[i] + R[i]);
                bl.push (softClip (in + fb * lpR.lowpass (yr)));
                br.push (softClip (fb * lpL.lowpass (yl)));
                L[i] += m * yl;
                R[i] += m * yr;
            }
            lastMix = mix;
        }

        DelayBuffer bl, br;
        Smoother time;
        OnePole lpL, lpR;
        Wander wow;
        float fs = 48000.0f, lastMix = 0.0f;
    };

    //==========================================================================
    // 8-line feedback delay network with input diffusion, damping, slow modulation
    // and an octave-up "shimmer" fed back into its input.
    struct Reverb
    {
        static constexpr int N = 8;

        void prepare (float sampleRate)
        {
            fs = sampleRate;
            static const float baseMs[N] = { 29.7f, 37.1f, 41.1f, 43.7f, 53.3f, 59.9f, 67.1f, 73.7f };
            for (int i = 0; i < N; ++i)
            {
                base[i] = baseMs[i] * 0.001f * fs;
                lines[i].allocate ((int) (base[i] * 2.2f) + 64);
                damp[i].clear();
            }
            static const float apMs[4] = { 4.7f, 3.6f, 12.7f, 9.3f };
            for (int i = 0; i < 4; ++i) ap[i].allocate ((int) (apMs[i] * 0.001f * fs));
            shifter.allocate (8192);
            shimHP.setCutoff (300.0f, fs);
            shimHP.clear();
            sizeSm.setTime (0.3f * fs);
            sizeSm.snap (1.0f);
            modPhase = 0.0f; shiftPhase = 0.0f; shimFeed = 0.0f;
        }

        void clear()
        {
            for (auto& l : lines) l.clear();
            for (auto& a : ap) a.clear();
            for (auto& d : damp) d.clear();
            shifter.clear(); shimHP.clear(); shimFeed = 0.0f;
        }

        void process (float* L, float* R, int n, float size, float decay, float dampAmt, float shimmer, float mix) noexcept
        {
            if (mix < 0.0005f && lastMix < 0.0005f && idleSamples > (int) (fs * 2.0f)) return;

            sizeSm.target = 0.5f + 1.5f * clamp01 (size);
            const float rt = reverbSec (decay);
            const float dampHz = 1500.0f * std::pow (2.0f, (1.0f - clamp01 (dampAmt)) * 3.5f);
            for (auto& d : damp) d.setCutoff (dampHz, fs);
            const float shim = clamp01 (shimmer);
            const float W = 3000.0f;
            float gain[N];
            for (int k = 0; k < N; ++k) gain[k] = std::pow (10.0f, -3.0f * base[k] * sizeSm.value / (rt * fs));

            for (int i = 0; i < n; ++i)
            {
                const float m = lastMix + (mix - lastMix) * ((float) i / (float) n);
                const float sz = sizeSm.next();

                // read the lines (two of them gently modulated)
                const float mod = std::sin (twoPi * modPhase);
                modPhase += 0.31f / fs; if (modPhase >= 1.0f) modPhase -= 1.0f;
                float y[N];
                for (int k = 0; k < N; ++k)
                {
                    float d = base[k] * sz;
                    if (k == 0) d += 12.0f * mod;
                    if (k == 5) d -= 9.0f * mod;
                    y[k] = lines[k].readHermite (d);
                }

                // stereo out
                float outL = 0.0f, outR = 0.0f;
                for (int k = 0; k < N; k += 2) { outL += y[k]; outR += y[k + 1]; }
                outL *= 0.35f; outR *= 0.35f;

                // shimmer: octave-up pitch shifter on the tail
                float shimOut = 0.0f;
                if (shim > 0.001f)
                {
                    shifter.push (0.5f * (outL + outR));
                    shiftPhase += 1.0f / W; if (shiftPhase >= 1.0f) shiftPhase -= 1.0f;
                    float p2 = shiftPhase + 0.5f; if (p2 >= 1.0f) p2 -= 1.0f;
                    const float t1 = shifter.readHermite (W * (1.0f - shiftPhase) + 4.0f);
                    const float t2 = shifter.readHermite (W * (1.0f - p2) + 4.0f);
                    shimOut = shimHP.highpass (t1 * std::sin (pi * shiftPhase) + t2 * std::sin (pi * p2));
                }

                // input: diffuse the mono sum, add the shimmer
                float in = 0.5f * (L[i] + R[i]) + shimFeed;
                for (auto& a : ap) in = a.process (in, 0.62f);
                shimFeed = shimOut * shim * 0.55f;

                // Hadamard mix + feedback
                float h[N];
                for (int k = 0; k < N; ++k) h[k] = y[k];
                for (int len = 1; len < N; len <<= 1)
                    for (int s = 0; s < N; s += len * 2)
                        for (int j = s; j < s + len; ++j)
                        {
                            const float a = h[j], b = h[j + len];
                            h[j] = a + b; h[j + len] = a - b;
                        }
                const float norm = 0.35355339f;   // 1/sqrt(8)
                for (int k = 0; k < N; ++k)
                {
                    const float fbk = damp[k].lowpass (h[k] * norm) * gain[k];
                    lines[k].push (fbk + (k % 2 == 0 ? in : -in) * 0.5f);
                }

                L[i] = L[i] * (1.0f - 0.25f * m) + m * outL;
                R[i] = R[i] * (1.0f - 0.25f * m) + m * outR;
            }

            lastMix = mix;
            idleSamples = mix < 0.0005f ? idleSamples + n : 0;
        }

        DelayBuffer lines[N];
        float base[N] {};
        OnePole damp[N];
        Allpass ap[4];
        DelayBuffer shifter;
        OnePole shimHP;
        Smoother sizeSm;
        float fs = 48000.0f, modPhase = 0.0f, shiftPhase = 0.0f, shimFeed = 0.0f, lastMix = 0.0f;
        int idleSamples = 0;
    };
}
