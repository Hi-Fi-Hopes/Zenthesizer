#pragma once

// One synth voice: Engine A (3 analog oscillators), Engine B (5 wavetable oscillators),
// breath noise, a 4-pole ladder filter per side and three envelopes.

#include "Common.h"
#include "Maps.h"
#include "Settings.h"
#include "Wavetables.h"

namespace zen
{
    inline float polyBlep (float t, float dt) noexcept
    {
        if (t < dt)        { t /= dt; return t + t - t * t - 1.0f; }
        if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
        return 0.0f;
    }

    inline float fastTanh (float x) noexcept
    {
        x = clampf (x, -4.5f, 4.5f);
        const float x2 = x * x;
        return x * (27.0f + x2) / (27.0f + 9.0f * x2);
    }

    //==========================================================================
    struct AnalogOsc
    {
        float phase = 0.0f, fb = 0.0f;

        float process (WaveA w, float shape, float dt) noexcept
        {
            float y = 0.0f;
            const float p = phase;
            switch (w)
            {
                case WaveA::saw:
                {
                    const float saw = 2.0f * p - 1.0f - polyBlep (p, dt);
                    const float tri = 1.0f - 4.0f * std::abs (p - 0.5f);
                    y = saw + (tri - saw) * shape;    // Shape softens the saw toward a triangle
                    break;
                }
                case WaveA::square:
                {
                    const float pw = 0.5f - 0.42f * shape;
                    float sq = p < pw ? 1.0f : -1.0f;
                    sq += polyBlep (p, dt);
                    float q = p - pw; if (q < 0.0f) q += 1.0f;
                    sq -= polyBlep (q, dt);
                    y = sq - (2.0f * pw - 1.0f);
                    break;
                }
                case WaveA::triangle:
                {
                    const float tri = 1.0f - 4.0f * std::abs (p - 0.5f);
                    if (shape > 0.001f)
                    {
                        const float folded = std::sin (0.5f * pi * tri * (1.0f + 3.0f * shape));   // West Coast fold
                        y = tri + (folded - tri) * std::min (1.0f, shape * 4.0f);
                    }
                    else y = tri;
                    break;
                }
                case WaveA::sine:
                default:
                {
                    const float s = std::sin (twoPi * p + fb * shape * 1.6f);   // self-FM brightens it
                    fb = 0.5f * (fb + s);
                    y = s;
                    break;
                }
            }
            phase += dt;
            if (phase >= 1.0f) phase -= 1.0f;
            return y;
        }
    };

    //==========================================================================
    // A plucked string (Karplus-Strong, extended). A burst of filtered noise circulates in a delay
    // line one period long; a linear-phase loop filter takes the highs away a little each pass.
    // SHAPE 0..0.66: soft nylon -> bright steel (brighter pluck, less damping, pluck nearer the bridge);
    // 0.66..1: a sitar-style buzzing bridge adds a bright fizz to each swing, for as long as the note rings.
    struct PluckString
    {
        static constexpr int size = 4096, mask = size - 1;   // lowest note ~12 Hz at 48 kHz
        float buf[size] {};
        int w = 0;
        float x1 = 0.0f, x2 = 0.0f, env = 0.0f, dcIn = 0.0f, dcOut = 0.0f, bzIn = 0.0f, bzOut = 0.0f;

        void clear() noexcept { std::fill (std::begin (buf), std::end (buf), 0.0f); x1 = x2 = env = dcIn = dcOut = bzIn = bzOut = 0.0f; }

        // Adds a pluck to whatever is already ringing (like plucking a ringing string again).
        void pluck (float periodSamples, float shape, float velocity, Rng& rng) noexcept
        {
            const int n = std::max (2, std::min (size - 2, (int) periodSamples));
            const float tone = std::min (1.0f, shape / 0.66f);
            const float bright = clamp01 (0.15f + 0.65f * tone + 0.3f * velocity);
            const float a = 0.08f + 0.9f * bright * bright;           // one-pole lowpass on the noise
            const int posD = std::max (1, (int) ((float) n * (0.5f - 0.38f * tone)));   // pluck position
            float tmp[size];
            float lp = 0.0f, mean = 0.0f;
            for (int k = 0; k < n; ++k) { lp += a * (rng.next() - lp); tmp[k] = lp; }
            for (int k = n - 1; k >= posD; --k) tmp[k] -= 0.8f * tmp[k - posD];   // comb: where along the string
            for (int k = 0; k < n; ++k) mean += tmp[k];
            mean /= (float) n;
            float pk = 1.0e-6f;
            for (int k = 0; k < n; ++k) { tmp[k] -= mean; pk = std::max (pk, std::abs (tmp[k])); }
            const float g = (0.35f + 0.65f * velocity) / pk;
            for (int k = 0; k < n; ++k) buf[(size_t) ((w - n + k) & mask)] += tmp[k] * g;
            env = std::max (env, 0.35f + 0.65f * velocity);
        }

        // periodSamples: fs / Hz. decay: per-period gain for the fundamental. damp: 0 bright .. 0.25 dark.
        float process (float periodSamples, float damp, float gain, float buzz) noexcept
        {
            const float d = std::max (1.0f, std::min ((float) size - 3.0f, periodSamples - 1.0f));   // the loop filter adds 1
            float rp = (float) w - d;
            if (rp < 0.0f) rp += (float) size;
            const int i0 = (int) rp;
            const float fr = rp - (float) i0;
            const float a = buf[(size_t) (i0 & mask)], b = buf[(size_t) ((i0 + 1) & mask)];
            const float x = a + (b - a) * fr;
            float y = (damp * x + (1.0f - 2.0f * damp) * x1 + damp * x2) * gain;
            x2 = x1; x1 = x;
            buf[(size_t) w] = y;
            w = (w + 1) & mask;
            float out = y;
            if (buzz > 0.0f)
            {
                // the buzzing bridge (jawari): each time the string swings onto the curved bridge its
                // peak gets a bright, fizzing edge. Measured against the string's own level, so the
                // buzz lasts as long as the note does, the way a sitar's does.
                env = std::max (std::abs (y), env * 0.9995f);
                const float u = y / std::max (1.0e-5f, env);
                const float touch = u > 0.4f ? std::tanh ((u - 0.4f) * 14.0f) : 0.0f;   // a quick, hard contact
                bzOut = touch - bzIn + 0.93f * bzOut; bzIn = touch;   // keep only the fizz (above ~500 Hz)
                out += buzz * 1.6f * bzOut * env;
            }
            // gentle DC blocker on the way out
            dcOut = out - dcIn + 0.9995f * dcOut; dcIn = out;
            return dcOut;
        }
    };

    //==========================================================================
    // 4-pole zero-delay ladder (Zavalishin), tanh on the input, resonance up to self-oscillation.
    struct Ladder
    {
        float s[4] {};

        float process (float x, float G, float k) noexcept
        {
            const float oneMinusG = 1.0f - G;
            const float G2 = G * G, G3 = G2 * G, G4 = G3 * G;
            const float sigma = G3 * oneMinusG * s[0] + G2 * oneMinusG * s[1] + G * oneMinusG * s[2] + oneMinusG * s[3];
            const float y4est = (G4 * x + sigma) / (1.0f + k * G4);
            float u = fastTanh (x - k * y4est);
            for (int i = 0; i < 4; ++i)
            {
                const float v = (u - s[i]) * G;
                const float y = v + s[i];
                s[i] = y + v;
                u = y;
            }
            return u * (1.0f + 0.55f * k);
        }

        void clear() noexcept { for (auto& v : s) v = 0.0f; }
    };

    //==========================================================================
    struct Env
    {
        enum Stage { idle, attack, decay, release };
        Stage stage = idle;
        float level = 0.0f;
        float aCoef = 1.0f, dCoef = 1.0f, rCoef = 1.0f, sus = 1.0f;

        void set (float a, float d, float s, float r, float fs) noexcept
        {
            aCoef = 1.0f - std::exp (-1.466f / (envTimeSec (a) * fs));
            dCoef = 1.0f - std::exp (-4.6f / (envTimeSec (d) * fs));
            rCoef = 1.0f - std::exp (-4.6f / (envTimeSec (r) * fs));
            sus = clamp01 (s);
        }
        void gateOn() noexcept  { stage = attack; }
        void gateOff() noexcept { if (stage != idle) stage = release; }
        void reset() noexcept   { stage = idle; level = 0.0f; }

        float next() noexcept
        {
            switch (stage)
            {
                case attack:
                    level += (1.3f - level) * aCoef;
                    if (level >= 1.0f) { level = 1.0f; stage = decay; }
                    break;
                case decay:   level += (sus - level) * dCoef; break;
                case release:
                    level += (0.0f - level) * rCoef;
                    if (level < 1.0e-4f) { level = 0.0f; stage = idle; }
                    break;
                case idle: default: break;
            }
            return level;
        }

        // Advance n samples at once (used for the mod envelopes at control rate).
        float advance (int n) noexcept
        {
            float v = level;
            for (int i = 0; i < n; ++i) v = next();
            return v;
        }
    };

    //==========================================================================
    struct Voice
    {
        static constexpr int numA = 3, numB = 5;

        bool active = false, gate = false, sustained = false;
        int id = -1;          // which key / plate / arp step owns it
        int plate = -1;       // note plate 0..11 if played from a plate
        float note = 60.0f;   // target pitch (semitones)
        float pitch = 60.0f;  // gliding pitch
        float vel = 0.8f;
        float pressTarget = 0.0f, press = 0.0f;
        float noteBend = 0.0f, noteBendTarget = 0.0f;   // MPE: this note's own pitch bend (semitones)
        float slide = 0.0f, slideTarget = 0.0f;         // MPE: this note's slide (CC74), 0..1
        uint32_t age = 0;

        AnalogOsc a[numA];
        PluckString str[2];          // oscillator A in STRING mode: a string and its detuned twin
        bool pluckPending = false;
        float bPhase[numB] {};
        Ladder lad[2];
        Env amp, e1, e2;
        Rng noise;
        OnePole noiseHP, noiseLP;

        // control-rate state (start/end of the current 32-sample chunk)
        float cutLog = 10.0f, cutLogTarget = 10.0f;
        float aGain = 0.0f, aGainT = 0.0f, bGain = 0.0f, bGainT = 0.0f, airG = 0.0f, airGT = 0.0f;
        float panL = 1.0f, panR = 1.0f, panLT = 1.0f, panRT = 1.0f;

        void start (int newId, float newNote, float velocity, int fromPlate, bool glide, uint32_t serial, uint32_t seed)
        {
            const bool wasActive = active;
            id = newId; plate = fromPlate; note = newNote; vel = velocity;
            if (! glide) pitch = newNote;   // with glide the caller has set where to glide from
            pressTarget = 0.0f;
            if (! wasActive) press = 0.0f;
            noteBendTarget = 0.0f; noteBend = 0.0f;
            if (! wasActive) slide = slideTarget;
            age = serial;
            gate = true; sustained = false; active = true;

            if (! wasActive)
            {
                // Free-running analog feel: random start phases, filter cleared.
                Rng r; r.s = seed | 1u;
                for (auto& o : a) { o.phase = 0.5f + 0.5f * r.next(); o.fb = 0.0f; }
                for (auto& p : bPhase) p = 0.5f + 0.5f * r.next();
                lad[0].clear(); lad[1].clear();
                str[0].clear(); str[1].clear();
                noise.s = seed * 2654435761u | 1u;
                amp.level = 0.0f; e1.level = 0.0f; e2.level = 0.0f;
            }
            amp.gateOn(); e1.gateOn(); e2.gateOn();
            pluckPending = true;
        }

        void release() noexcept
        {
            gate = false;
            amp.gateOff(); e1.gateOff(); e2.gateOff();
        }
    };
}
