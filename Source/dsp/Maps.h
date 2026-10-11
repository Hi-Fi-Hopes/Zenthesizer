#pragma once

// How the 0..1 knob values turn into real units. Shared by the engine and the UI readouts.

#include "Common.h"

namespace zen
{
    inline float envTimeSec (float v)     { return 0.001f * std::pow (15000.0f, clamp01 (v)); }   // 1 ms .. 15 s
    inline float cutoffHz (float v)       { return 20.0f * std::pow (900.0f, clamp01 (v)); }      // 20 Hz .. 18 kHz
    inline float lfoHz (float v)          { return 0.02f * std::pow (1000.0f, clamp01 (v)); }     // 0.02 .. 20 Hz
    inline float tuneSemis (float v)      { return (clamp01 (v) - 0.5f) * 48.0f; }               // -24 .. +24
    inline float glideSec (float v)       { return v <= 0.001f ? 0.0f : 0.002f * std::pow (1000.0f, clamp01 (v)); }
    inline float delayMs (float v)        { return 40.0f * std::pow (37.5f, clamp01 (v)); }       // 40 ms .. 1.5 s
    inline float reverbSec (float v)      { return 0.4f * std::pow (75.0f, clamp01 (v)); }        // 0.4 .. 30 s
    inline float volumeDb (float v)       { return -36.0f + clamp01 (v) * 42.0f; }                // -36 .. +6 dB (0.857 = 0 dB)
    inline float bipolar (float v)        { return (clamp01 (v) - 0.5f) * 2.0f; }

    // Snap tune to whole semitones (fine detune lives on the Detune knob).
    inline float tuneSemisSnapped (float v) { return std::round (tuneSemis (v)); }

    // Note divisions for the arp and the synced delay, in quarter notes.
    inline float arpBeats (int i)
    {
        static const float b[] = { 1.0f, 0.5f, 1.0f / 3.0f, 0.25f, 1.0f / 6.0f, 0.125f };
        return b[std::min (5, std::max (0, i))];
    }
    inline float delaySyncBeats (int i)   // 0 = free
    {
        static const float b[] = { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f, 1.5f, 2.0f };
        return b[std::min (6, std::max (0, i))];
    }

    // Scales for the note plates (intervals from the root).
    struct Scale { const char* name; int n; int steps[12]; };
    inline const Scale& scaleAt (int i)
    {
        static const Scale s[] = {
            { "Chromatic",   12, { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 } },
            { "Major",        7, { 0, 2, 4, 5, 7, 9, 11 } },
            { "Minor",        7, { 0, 2, 3, 5, 7, 8, 10 } },
            { "Dorian",       7, { 0, 2, 3, 5, 7, 9, 10 } },
            { "Lydian",       7, { 0, 2, 4, 6, 7, 9, 11 } },
            { "Major Pent",   5, { 0, 2, 4, 7, 9 } },
            { "Minor Pent",   5, { 0, 3, 5, 7, 10 } },
            { "Hirajoshi",    5, { 0, 2, 3, 7, 8 } },
            { "In-Sen",       5, { 0, 1, 5, 7, 10 } },
            { "Celtic",       5, { 0, 2, 3, 7, 9 } },
            { "Whole Tone",   6, { 0, 2, 4, 6, 8, 10 } },
        };
        return s[std::min (10, std::max (0, i))];
    }

    // MIDI note for note plate p (0..11).
    inline int plateNote (int p, int root, int scaleIndex, int octave)
    {
        const auto& sc = scaleAt (scaleIndex);
        return 48 + root + octave * 12 + sc.steps[p % sc.n] + 12 * (p / sc.n);
    }
}
