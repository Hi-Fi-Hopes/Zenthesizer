#pragma once
#include <algorithm>
#include <string>
#include <vector>

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

    // The keys are chromatic, piano-style: key 0 is middle C (C3 in Ableton's naming), moved by the octave setting.
    inline int keyNote (int key, int octave) { return 60 + octave * 12 + key; }

    inline bool inScale (int note, int root, int scaleIndex)
    {
        const auto& sc = scaleAt (scaleIndex);
        const int pc = ((note - root) % 12 + 12) % 12;
        for (int i = 0; i < sc.n; ++i) if (sc.steps[i] == pc) return true;
        return false;
    }

    // The nearest note in the scale (a tie goes up).
    inline int snapToScale (int note, int root, int scaleIndex)
    {
        for (int d = 0; d < 12; ++d)
        {
            if (inScale (note + d, root, scaleIndex)) return note + d;
            if (inScale (note - d - 1, root, scaleIndex) && ! inScale (note + d + 1, root, scaleIndex)) return note - d - 1;
        }
        return note;
    }

    //==========================================================================
    // Chords: the seven chords of the key, built by stacking thirds inside a seven-note scale.
    // Five-note and other scales borrow the seven-note scale they sit in (minor pentatonic -> minor).
    inline const Scale& chordScale (int scaleIndex)
    {
        static const Scale major { "Major", 7, { 0, 2, 4, 5, 7, 9, 11 } }, minor { "Minor", 7, { 0, 2, 3, 5, 7, 8, 10 } },
                           dorian { "Dorian", 7, { 0, 2, 3, 5, 7, 9, 10 } }, lydian { "Lydian", 7, { 0, 2, 4, 6, 7, 9, 11 } },
                           phrygian { "Phrygian", 7, { 0, 1, 3, 5, 7, 8, 10 } };
        switch (std::min (10, std::max (0, scaleIndex)))
        {
            case 2: case 6: case 7: return minor;   // Minor, Minor Pent, Hirajoshi
            case 3: case 9: return dorian;          // Dorian, Celtic
            case 4: case 10: return lydian;         // Lydian, Whole Tone
            case 8: return phrygian;                // In-Sen
            default: return major;                  // Chromatic, Major, Major Pent
        }
    }

    struct Chord
    {
        int notes[6] {};
        int n = 0;
        int rootNote = 48;
        int third = 4, fifth = 7, seventh = -1, ninth = -1;   // intervals above the root (-1 = not in it); third 2/5 = sus
    };

    // degree 0..6; octave as the keys' octave setting
    inline Chord buildChord (int degree, int root, int scaleIndex, int octave, bool add7, bool add9, bool sus, bool spread)
    {
        const auto& sc = chordScale (scaleIndex);
        auto step = [&] (int k) { return sc.steps[k % 7] + 12 * (k / 7); };   // scale step k above the key's root
        degree = std::min (6, std::max (0, degree));
        Chord c;
        int base = 60 + root + octave * 12 + step (degree);
        if (base > 60 + octave * 12 + 7) base -= 12;      // keep chords around the middle of the keys
        c.rootNote = base;
        const int at = step (degree);
        c.third = step (degree + 2) - at;
        c.fifth = step (degree + 4) - at;
        if (sus)
        {
            // sus2 where the second is a whole step, otherwise sus4 where the fourth is perfect
            const int second = step (degree + 1) - at, fourth = step (degree + 3) - at;
            if (second == 2) c.third = 2; else if (fourth == 5) c.third = 5;
        }
        if (add7) c.seventh = step (degree + 6) - at;
        if (add9) c.ninth = step (degree + 8) - at;

        std::vector<int> iv;
        if (spread)
        {
            // an open voicing for pads: the root an octave down, the third an octave up
            iv.push_back (-12); iv.push_back (c.fifth); iv.push_back (c.third + 12);
            if (c.seventh >= 0) iv.push_back (c.seventh + 12);
            if (c.ninth >= 0) iv.push_back (c.ninth + 12);
        }
        else
        {
            iv.push_back (0); iv.push_back (c.third); iv.push_back (c.fifth);
            if (c.seventh >= 0) iv.push_back (c.seventh);
            if (c.ninth >= 0) iv.push_back (c.ninth);
        }
        std::sort (iv.begin(), iv.end());
        for (int x : iv) if (c.n < 6) c.notes[c.n++] = base + x;
        return c;
    }

    inline const char* noteName (int pc, bool flats)
    {
        static const char* sh[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        static const char* fl[] = { "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B" };
        return (flats ? fl : sh)[((pc % 12) + 12) % 12];
    }

    // Spell chord names with flats in flat keys (C minor: Eb, Ab, Bb), sharps otherwise.
    inline bool keyUsesFlats (int root, int scaleIndex)
    {
        const std::string n = chordScale (scaleIndex).name;
        const int toMajor = n == "Minor" ? 3 : n == "Dorian" ? 10 : n == "Phrygian" ? 8 : n == "Lydian" ? 7 : 0;
        const int rel = (root + toMajor) % 12;
        return rel == 5 || rel == 10 || rel == 3 || rel == 8 || rel == 1;
    }

    // "Cm7", "Ebmaj9", "Gsus2", "Bdim" ...
    inline std::string chordName (const Chord& c, bool flats = false)
    {
        std::string s = noteName (c.rootNote, flats);
        const bool sus = c.third == 2 || c.third == 5;
        const bool minor = c.third == 3, dim = minor && c.fifth == 6, aug = c.third == 4 && c.fifth == 8;
        const char* ext = c.ninth >= 0 && c.seventh >= 0 ? "9" : (c.seventh >= 0 ? "7" : "");
        if (sus)
        {
            if (c.seventh >= 0) s += c.seventh == 11 ? std::string ("maj") + ext : ext;
            s += c.third == 2 ? "sus2" : "sus4";
        }
        else if (dim)
        {
            if (c.seventh == 10) s += std::string ("m") + ext + "b5";
            else if (c.seventh == 9) s += "dim7";
            else s += "dim";
        }
        else if (aug) s += std::string ("+") + ext;
        else if (minor) s += c.seventh == 11 ? std::string ("m(maj") + ext + ")" : std::string ("m") + ext;
        else s += c.seventh == 11 ? std::string ("maj") + ext : std::string (ext);
        if (c.ninth >= 0 && c.seventh < 0) s += "add9";
        return s;
    }

    // The chord's Roman numeral in the key: upper case major, lower case minor, ° diminished, + augmented.
    inline std::string chordNumeral (int degree, int scaleIndex)
    {
        static const char* up[] = { "I", "II", "III", "IV", "V", "VI", "VII" };
        static const char* lo[] = { "i", "ii", "iii", "iv", "v", "vi", "vii" };
        const auto c = buildChord (degree, 0, scaleIndex, 0, false, false, false, false);
        degree = std::min (6, std::max (0, degree));
        if (c.third == 3) return std::string (lo[degree]) + (c.fifth == 6 ? "\xc2\xb0" : "");
        return std::string (up[degree]) + (c.fifth == 8 ? "+" : "");
    }
}
