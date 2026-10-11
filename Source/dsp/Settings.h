#pragma once

// Everything the engine needs to know from the plugin's parameters, read once per block.
// Knob-style values are 0..1 exactly as the knobs show them; the engine maps them to Hz, seconds, etc.

#include <array>

namespace zen
{
    // ---- Modulation -----------------------------------------------------------
    enum class Src : int
    {
        none = 0, env1, env2, lfo1, lfo2, rand, press, vel, wheel, chanAT,
        plateA, plateB, plateC, plateD, key,
        count
    };

    enum class Dst : int
    {
        none = 0, pitch, aTune, aDetune, aShape, aLevel, bPos, bWarp, bUnison, bLevel, air,
        cutoff, res, drive, amp, pan, lfo1Rate, lfo2Rate, chorusMix, delayMix, reverbMix,
        count
    };

    constexpr int numSrc = (int) Src::count;
    constexpr int numDst = (int) Dst::count;
    constexpr int numSlots = 12;

    inline bool isGlobalDst (Dst d) noexcept
    {
        return d == Dst::lfo1Rate || d == Dst::lfo2Rate || d == Dst::chorusMix || d == Dst::delayMix || d == Dst::reverbMix;
    }

    struct ModSlot
    {
        Src src = Src::none;
        Dst dst = Dst::none;
        float amount = 0.0f;   // -1..1
    };

    // ---- Choices --------------------------------------------------------------
    enum class WaveA : int { saw = 0, square, triangle, sine };
    enum class LfoShape : int { sine = 0, triangle, saw, square, drift };
    enum class ArpMode : int { up = 0, down, upDown, random, played };

    constexpr int numTables = 4;          // built-in tables (AIR, GLASS, VOX, REED); 4 = the user's own
    constexpr int numScales = 11;
    constexpr int numPlates = 16;   // 12 note plates + 4 timbre plates (A-D)
    constexpr int numNotePlates = 12;

    struct Settings
    {
        // Engine A (analog)
        WaveA aWave = WaveA::saw;
        float aTune = 0.5f;     // 0.5 = no offset; 0..1 -> -24..+24 semitones
        float aDetune = 0.2f, aShape = 0.0f, aLevel = 0.8f;

        // Engine B (wavetable)
        int   bTable = 0;
        float bPos = 0.3f, bWarp = 0.0f, bUnison = 0.0f, bLevel = 0.0f;

        float air = 0.0f;       // breath noise

        // Filter + amp
        float cutoff = 0.7f, res = 0.2f, drive = 0.1f, envAmt = 0.5f;   // envAmt 0.5 = none (bipolar)
        float keyTrack = 0.5f;
        float ampA = 0.05f, ampD = 0.5f, ampS = 0.8f, ampR = 0.45f;

        // Mod envelopes and LFOs
        float e1A = 0.05f, e1D = 0.45f, e1S = 0.3f, e1R = 0.45f;
        float e2A = 0.5f,  e2D = 0.6f,  e2S = 0.6f, e2R = 0.6f;
        float lfo1Rate = 0.35f, lfo2Rate = 0.2f;
        LfoShape lfo1Shape = LfoShape::sine, lfo2Shape = LfoShape::drift;
        float randRate = 0.4f, randSmooth = 0.5f;
        std::array<ModSlot, numSlots> slots {};

        // Global voice settings
        float glide = 0.0f, spread = 0.5f, volume = 0.857f, velSens = 0.5f;

        // FX
        float chorusMix = 0.0f, chorusDepth = 0.5f, chorusRate = 0.3f;
        float delayTime = 0.45f, delayFeedback = 0.35f, delayTone = 0.5f, delayMix = 0.0f;
        int   delaySync = 0;    // 0 = free (ms), else a note division
        float reverbSize = 0.6f, reverbDecay = 0.5f, reverbDamp = 0.5f, reverbShimmer = 0.0f, reverbMix = 0.2f;

        // Play: note plates, pads, arpeggiator
        int  root = 0, scale = 6, octave = 0;      // scale 6 = minor pentatonic
        bool latchTimbre = false;
        int  padChannel = 0;                       // 0 = any channel
        bool platesFromPads = true;
        std::array<int, numPlates> padNotes {};    // MIDI note that plays each plate (-1 = none)
        bool arpOn = false;
        ArpMode arpMode = ArpMode::up;
        int  arpRate = 3, arpOctaves = 1;
        float arpGate = 0.6f;
        bool arpHold = false;

        float loopLevel = 0.8f;   // the looper's playback level

        float bpm = 0.0f;   // host tempo, 0 if unknown
    };

    // Launchkey drum-mode style default: 16 pads send notes 36..51.
    // 36..47 play note plates 1..12, 48..51 are the timbre plates A..D.
    inline std::array<int, numPlates> defaultPadNotes()
    {
        std::array<int, numPlates> a {};
        for (int i = 0; i < numPlates; ++i) a[(size_t) i] = 36 + i;
        return a;
    }
}
