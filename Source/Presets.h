#pragma once

// Factory presets. Values are plain parameter values: knobs 0..1, choices by index, switches 0/1.
// Every preset starts from the defaults plus the "plate kit" below, then applies its own values.
//
// Handy knob values:
//   cutoff  0.5 = 600 Hz, 0.6 = 1.2 kHz, 0.7 = 2.3 kHz, 0.8 = 4.3 kHz
//   times   0.3 = 18 ms, 0.5 = 120 ms, 0.6 = 0.3 s, 0.7 = 0.8 s, 0.8 = 2.2 s, 0.9 = 5.7 s
//   tune    0.25 = -12, 0.5 = 0, 0.6458 = +7, 0.75 = +12, 0.8958 = +19, 1.0 = +24 semitones
//   lfo     0.3 = 0.16 Hz, 0.5 = 0.63 Hz, 0.6 = 1.3 Hz, 0.7 = 2.5 Hz, 0.75 = 3.6 Hz
//   envAmt  0.5 = none, 0.7 = +2.4 octaves of filter sweep
//   delaySync 0 free, 1 1/16, 2 1/8, 3 1/8D, 4 1/4, 5 1/4D, 6 1/2
//   arpRate 0 1/4, 1 1/8, 2 1/8T, 3 1/16, 4 1/16T, 5 1/32

#include "PluginProcessor.h"
#include <vector>

namespace presets
{
    using zen::Src; using zen::Dst;

    struct Value { const char* id; float v; };
    struct Slot  { Src s; Dst d; float amt; };
    struct Preset
    {
        const char* name;
        const char* category;
        std::vector<Value> values;
        std::vector<Slot> slots;   // fill slots 8..12 (the plate kit uses 1..7)
    };

    // The plate kit: what the pads and plates do in every preset unless a preset changes it.
    inline std::vector<Slot> kit()
    {
        return {
            { Src::press,  Dst::cutoff,    0.25f },   // pressing a note plate opens it up
            { Src::plateA, Dst::cutoff,    0.35f },   // A: bloom
            { Src::plateB, Dst::bPos,      0.50f },   // B: morph the wavetable
            { Src::plateC, Dst::delayMix,  0.50f },   // C: echoes
            { Src::plateD, Dst::reverbMix, 0.50f },   // D: space
            { Src::wheel,  Dst::cutoff,    0.30f },
            { Src::chanAT, Dst::cutoff,    0.20f },   // KeyLab-style channel pressure
        };
    }

    #include "PresetsMore.h"

    // The first batch (below) followed by the second (PresetsMore.h). New presets go at the end,
    // so a saved session's preset number keeps pointing at the same sound.
    inline std::vector<Preset> withMore (std::vector<Preset> v)
    {
        auto more = morePresets();
        v.insert (v.end(), more.begin(), more.end());
        return v;
    }

    inline const std::vector<Preset>& all()
    {
        using namespace ids;
        static const std::vector<Preset> list = withMore ({
            { "Init", "Init", {}, {} },

            //================================================================ ETHEREAL
            { "Temple Mist", "Ethereal", {
                { bTable, 0 }, { bPos, 0.35f }, { bWarp, 0.1f }, { bUnison, 0.6f }, { bLevel, 0.8f },
                { aWave, 3 }, { aLevel, 0.25f }, { aTune, 0.75f }, { aDetune, 0.1f }, { air, 0.25f },
                { cutoff, 0.62f }, { res, 0.15f }, { envAmt, 0.62f }, { e1A, 0.85f }, { e1D, 0.8f }, { e1S, 0.6f },
                { ampA, 0.8f }, { ampD, 0.7f }, { ampS, 0.85f }, { ampR, 0.85f },
                { lfo1Rate, 0.3f }, { lfo2Rate, 0.35f },
                { chorusMix, 0.4f }, { delaySync, 4 }, { delayFeedback, 0.4f }, { delayMix, 0.2f }, { delayTone, 0.4f },
                { reverbSize, 0.85f }, { reverbDecay, 0.8f }, { reverbDamp, 0.45f }, { reverbShimmer, 0.35f }, { reverbMix, 0.55f } },
              { { Src::lfo1, Dst::bPos, 0.25f }, { Src::lfo2, Dst::pitch, 0.006f } } },

            { "Halo Breath", "Ethereal", {
                { bTable, 2 }, { bPos, 0.2f }, { bUnison, 0.5f }, { bLevel, 0.8f }, { aWave, 2 }, { aLevel, 0.3f },
                { air, 0.35f }, { cutoff, 0.7f }, { res, 0.1f },
                { ampA, 0.75f }, { ampD, 0.7f }, { ampS, 0.9f }, { ampR, 0.85f }, { lfo1Rate, 0.25f },
                { chorusMix, 0.3f }, { reverbSize, 0.8f }, { reverbDecay, 0.75f }, { reverbShimmer, 0.5f }, { reverbMix, 0.6f } },
              { { Src::lfo1, Dst::bPos, 0.5f } } },

            { "Glass Cathedral", "Ethereal", {
                { bTable, 1 }, { bPos, 0.6f }, { bUnison, 0.5f }, { bLevel, 0.75f },
                { aWave, 3 }, { aTune, 0.75f }, { aShape, 0.3f }, { aLevel, 0.3f },
                { cutoff, 0.8f }, { ampA, 0.5f }, { ampD, 0.85f }, { ampS, 0.6f }, { ampR, 0.9f },
                { delaySync, 5 }, { delayFeedback, 0.45f }, { delayMix, 0.25f },
                { reverbSize, 1.0f }, { reverbDecay, 0.9f }, { reverbShimmer, 0.6f }, { reverbMix, 0.6f } },
              { { Src::lfo2, Dst::bPos, 0.15f } } },

            { "Forest Floor", "Ethereal", {
                { aWave, 0 }, { aShape, 0.7f }, { aDetune, 0.35f }, { aLevel, 0.6f },
                { bTable, 0 }, { bPos, 0.5f }, { bLevel, 0.4f }, { air, 0.3f },
                { cutoff, 0.48f }, { res, 0.3f }, { envAmt, 0.65f }, { e1A, 0.7f }, { e1D, 0.8f }, { e1S, 0.4f },
                { ampA, 0.7f }, { ampS, 0.85f }, { ampR, 0.8f }, { lfo2Rate, 0.35f },
                { chorusMix, 0.35f }, { reverbSize, 0.7f }, { reverbDecay, 0.7f }, { reverbMix, 0.45f } },
              { { Src::lfo2, Dst::cutoff, 0.12f } } },

            { "Northern Lights", "Ethereal", {
                { volume, 0.93f }, { bTable, 1 }, { bPos, 0.3f }, { bUnison, 0.8f }, { bWarp, 0.2f }, { bLevel, 0.7f },
                { aWave, 0 }, { aDetune, 0.6f }, { aLevel, 0.35f }, { aShape, 0.4f },
                { cutoff, 0.66f }, { res, 0.25f }, { ampA, 0.82f }, { ampS, 0.9f }, { ampR, 0.88f },
                { lfo1Rate, 0.2f }, { lfo2Rate, 0.3f },
                { delaySync, 3 }, { delayFeedback, 0.5f }, { delayMix, 0.25f },
                { reverbSize, 1.0f }, { reverbDecay, 0.85f }, { reverbShimmer, 0.45f }, { reverbMix, 0.6f } },
              { { Src::lfo1, Dst::bPos, 0.4f }, { Src::lfo2, Dst::pan, 0.4f } } },

            { "Moss & Light", "Ethereal", {
                { aWave, 2 }, { aShape, 0.25f }, { aLevel, 0.5f }, { bTable, 0 }, { bPos, 0.55f }, { bLevel, 0.5f }, { bUnison, 0.3f },
                { cutoff, 0.58f }, { ampA, 0.6f }, { ampD, 0.75f }, { ampS, 0.7f }, { ampR, 0.8f }, { lfo1Rate, 0.45f },
                { chorusMix, 0.5f }, { chorusDepth, 0.6f }, { delaySync, 4 }, { delayFeedback, 0.3f }, { delayMix, 0.15f },
                { reverbSize, 0.65f }, { reverbDecay, 0.65f }, { reverbMix, 0.45f } },
              { { Src::lfo1, Dst::aShape, 0.2f } } },

            { "Ocean Floor", "Ethereal", {
                { aWave, 0 }, { aTune, 0.25f }, { aLevel, 0.55f }, { aDetune, 0.4f },
                { bTable, 2 }, { bPos, 0.7f }, { bLevel, 0.5f },
                { cutoff, 0.42f }, { res, 0.35f }, { e2A, 0.85f }, { e2D, 0.85f }, { e2S, 0.5f },
                { ampA, 0.8f }, { ampS, 0.9f }, { ampR, 0.85f },
                { reverbSize, 0.9f }, { reverbDecay, 0.85f }, { reverbDamp, 0.7f }, { reverbMix, 0.5f } },
              { { Src::env2, Dst::cutoff, 0.25f }, { Src::lfo2, Dst::bPos, 0.2f } } },

            { "Dream Choir", "Ethereal", {
                { aLevel, 0.0f }, { bTable, 2 }, { bPos, 0.1f }, { bUnison, 0.9f }, { bLevel, 0.85f }, { air, 0.2f },
                { cutoff, 0.72f }, { ampA, 0.78f }, { ampS, 0.95f }, { ampR, 0.85f }, { lfo1Rate, 0.15f },
                { chorusMix, 0.45f }, { reverbSize, 0.9f }, { reverbDecay, 0.8f }, { reverbShimmer, 0.3f }, { reverbMix, 0.6f } },
              { { Src::lfo1, Dst::bPos, 0.7f }, { Src::lfo2, Dst::pitch, 0.004f } } },

            { "Sky Lantern", "Ethereal", {
                { aWave, 1 }, { aShape, 0.3f }, { aDetune, 0.3f }, { aLevel, 0.4f }, { aTune, 0.75f },
                { bTable, 0 }, { bPos, 0.8f }, { bLevel, 0.5f },
                { cutoff, 0.64f }, { envAmt, 0.6f }, { e1A, 0.6f }, { e1D, 0.7f }, { e1S, 0.5f },
                { ampA, 0.7f }, { ampS, 0.85f }, { ampR, 0.8f }, { lfo1Rate, 0.4f },
                { delaySync, 3 }, { delayFeedback, 0.55f }, { delayMix, 0.3f },
                { reverbSize, 0.75f }, { reverbDecay, 0.75f }, { reverbShimmer, 0.4f }, { reverbMix, 0.5f } },
              { { Src::lfo1, Dst::aShape, 0.3f } } },

            { "Stillwater", "Ethereal", {
                { aWave, 3 }, { aLevel, 0.6f }, { aShape, 0.15f }, { aDetune, 0.05f },
                { bTable, 1 }, { bPos, 0.15f }, { bLevel, 0.35f },
                { cutoff, 0.75f }, { ampA, 0.65f }, { ampD, 0.9f }, { ampS, 0.5f }, { ampR, 0.9f },
                { delayTime, 0.7f }, { delayFeedback, 0.45f }, { delayMix, 0.2f },
                { reverbSize, 0.8f }, { reverbDecay, 0.9f }, { reverbDamp, 0.6f }, { reverbShimmer, 0.2f }, { reverbMix, 0.55f } },
              { { Src::lfo2, Dst::pitch, 0.004f } } },

            { "Aurora Shimmer", "Ethereal", {
                { volume, 1.0f }, { bTable, 1 }, { bPos, 0.45f }, { bUnison, 0.7f }, { bLevel, 0.7f },
                { aWave, 3 }, { aTune, 0.6458f }, { aLevel, 0.2f },
                { cutoff, 0.74f }, { ampA, 0.85f }, { ampS, 0.95f }, { ampR, 0.9f }, { lfo1Rate, 0.3f },
                { reverbSize, 1.0f }, { reverbDecay, 0.95f }, { reverbShimmer, 0.85f }, { reverbMix, 0.7f } },
              { { Src::lfo1, Dst::bWarp, 0.3f } } },

            { "Whale Song", "Ethereal", {
                { aWave, 3 }, { aShape, 0.5f }, { aTune, 0.25f }, { aLevel, 0.55f }, { glide, 0.55f },
                { bTable, 2 }, { bPos, 0.6f }, { bLevel, 0.4f },
                { cutoff, 0.5f }, { res, 0.45f }, { ampA, 0.7f }, { ampS, 0.9f }, { ampR, 0.85f }, { lfo1Rate, 0.5f },
                { delaySync, 5 }, { delayFeedback, 0.6f }, { delayMix, 0.3f }, { delayTone, 0.3f },
                { reverbSize, 0.9f }, { reverbDecay, 0.85f }, { reverbMix, 0.55f } },
              { { Src::lfo1, Dst::pitch, 0.01f } } },

            { "Spirit Drone", "Ethereal", {
                { aWave, 0 }, { aDetune, 0.7f }, { aLevel, 0.5f }, { aTune, 0.25f },
                { bTable, 3 }, { bPos, 0.2f }, { bLevel, 0.35f }, { bUnison, 0.5f }, { air, 0.2f },
                { cutoff, 0.38f }, { res, 0.5f }, { ampA, 0.8f }, { ampS, 1.0f }, { ampR, 0.9f },
                { lfo1Rate, 0.1f }, { lfo2Rate, 0.2f },
                { reverbSize, 0.9f }, { reverbDecay, 0.9f }, { reverbMix, 0.5f } },
              { { Src::lfo2, Dst::cutoff, 0.2f }, { Src::lfo1, Dst::bPos, 0.5f } } },

            { "Cloud Garden", "Ethereal", {
                { bTable, 0 }, { bPos, 0.4f }, { bUnison, 0.5f }, { bLevel, 0.6f }, { aWave, 2 }, { aLevel, 0.4f },
                { arpOn, 1 }, { arpMode, 3 }, { arpRate, 3 }, { arpOctaves, 1 }, { arpGate, 0.5f },
                { cutoff, 0.66f }, { ampA, 0.35f }, { ampD, 0.6f }, { ampS, 0.4f }, { ampR, 0.7f },
                { delaySync, 3 }, { delayFeedback, 0.55f }, { delayMix, 0.35f },
                { reverbSize, 0.8f }, { reverbDecay, 0.75f }, { reverbShimmer, 0.4f }, { reverbMix, 0.55f } },
              { { Src::rand, Dst::pan, 0.5f }, { Src::rand, Dst::bPos, 0.3f } } },

            { "Weightless", "Ethereal", {
                { volume, 1.0f }, { bTable, 1 }, { bPos, 0.8f }, { bLevel, 0.6f }, { bWarp, 0.3f },
                { aWave, 3 }, { aTune, 0.75f }, { aLevel, 0.3f }, { air, 0.15f },
                { cutoff, 0.78f }, { ampA, 0.9f }, { ampS, 1.0f }, { ampR, 0.92f }, { lfo1Rate, 0.2f },
                { chorusMix, 0.4f }, { reverbSize, 1.0f }, { reverbDecay, 0.9f }, { reverbShimmer, 0.6f }, { reverbMix, 0.65f } },
              { { Src::lfo1, Dst::amp, -0.2f } } },

            { "Sacred Grove", "Ethereal", {
                { aWave, 0 }, { aShape, 0.5f }, { aDetune, 0.3f }, { aLevel, 0.45f },
                { bTable, 2 }, { bPos, 0.35f }, { bLevel, 0.5f }, { bUnison, 0.4f },
                { cutoff, 0.55f }, { res, 0.2f }, { envAmt, 0.6f }, { e1A, 0.75f }, { e1D, 0.8f }, { e1S, 0.5f },
                { ampA, 0.75f }, { ampS, 0.9f }, { ampR, 0.85f }, { lfo1Rate, 0.25f },
                { delaySync, 4 }, { delayFeedback, 0.35f }, { delayMix, 0.15f },
                { reverbSize, 0.85f }, { reverbDecay, 0.8f }, { reverbShimmer, 0.25f }, { reverbMix, 0.5f } },
              { { Src::lfo1, Dst::bPos, 0.3f } } },

            //================================================================ KEYS
            { "Zen Rhodes", "Keys", {
                { aWave, 3 }, { aShape, 0.45f }, { aLevel, 0.7f }, { aDetune, 0.05f },
                { bTable, 1 }, { bPos, 0.1f }, { bLevel, 0.25f },
                { cutoff, 0.55f }, { envAmt, 0.62f }, { e1A, 0.0f }, { e1D, 0.55f }, { e1S, 0.0f }, { e1R, 0.5f },
                { ampA, 0.05f }, { ampD, 0.72f }, { ampS, 0.25f }, { ampR, 0.55f }, { velSens, 0.8f },
                { lfo1Rate, 0.6f }, { chorusMix, 0.35f }, { reverbSize, 0.55f }, { reverbDecay, 0.5f }, { reverbMix, 0.3f } },
              { { Src::vel, Dst::aShape, 0.3f }, { Src::lfo1, Dst::pan, 0.25f } } },

            { "Wooden Keys", "Keys", {
                { aWave, 2 }, { aShape, 0.3f }, { aLevel, 0.7f }, { bTable, 3 }, { bPos, 0.3f }, { bLevel, 0.25f },
                { cutoff, 0.5f }, { envAmt, 0.72f }, { e1A, 0.0f }, { e1D, 0.5f }, { e1S, 0.1f }, { e1R, 0.5f },
                { ampA, 0.02f }, { ampD, 0.65f }, { ampS, 0.3f }, { ampR, 0.5f }, { velSens, 0.7f },
                { reverbSize, 0.5f }, { reverbDecay, 0.45f }, { reverbMix, 0.25f } },
              { { Src::vel, Dst::cutoff, 0.15f } } },

            { "Felt Piano", "Keys", {
                { aWave, 2 }, { aShape, 0.1f }, { aLevel, 0.6f }, { bTable, 0 }, { bPos, 0.4f }, { bLevel, 0.4f }, { air, 0.1f },
                { cutoff, 0.45f }, { envAmt, 0.7f }, { e1A, 0.0f }, { e1D, 0.6f }, { e1S, 0.15f }, { e1R, 0.55f },
                { ampA, 0.04f }, { ampD, 0.75f }, { ampS, 0.35f }, { ampR, 0.6f }, { velSens, 0.8f },
                { chorusMix, 0.15f }, { reverbSize, 0.6f }, { reverbDecay, 0.55f }, { reverbDamp, 0.7f }, { reverbMix, 0.35f } },
              { { Src::vel, Dst::cutoff, 0.2f } } },

            { "Dream Wurli", "Keys", {
                { aWave, 1 }, { aShape, 0.25f }, { aLevel, 0.55f }, { aDetune, 0.1f },
                { cutoff, 0.52f }, { res, 0.25f }, { envAmt, 0.7f }, { e1A, 0.0f }, { e1D, 0.5f }, { e1S, 0.2f },
                { ampA, 0.03f }, { ampD, 0.7f }, { ampS, 0.3f }, { ampR, 0.55f }, { velSens, 0.75f },
                { lfo1Rate, 0.72f }, { chorusMix, 0.3f }, { reverbSize, 0.5f }, { reverbDecay, 0.5f }, { reverbMix, 0.25f } },
              { { Src::lfo1, Dst::amp, 0.12f }, { Src::vel, Dst::drive, 0.3f } } },

            { "Glass Piano", "Keys", {
                { bTable, 1 }, { bPos, 0.3f }, { bLevel, 0.6f }, { aWave, 3 }, { aTune, 0.75f }, { aShape, 0.3f }, { aLevel, 0.3f },
                { cutoff, 0.7f }, { envAmt, 0.65f }, { e1A, 0.0f }, { e1D, 0.55f }, { e1S, 0.1f },
                { ampA, 0.02f }, { ampD, 0.78f }, { ampS, 0.1f }, { ampR, 0.65f }, { velSens, 0.7f },
                { delaySync, 3 }, { delayFeedback, 0.3f }, { delayMix, 0.15f },
                { reverbSize, 0.7f }, { reverbDecay, 0.65f }, { reverbShimmer, 0.3f }, { reverbMix, 0.4f } },
              {} },

            { "Kalimba Dust", "Keys", {
                { aWave, 3 }, { aTune, 0.75f }, { aShape, 0.55f }, { aLevel, 0.6f }, { bTable, 1 }, { bPos, 0.55f }, { bLevel, 0.3f },
                { cutoff, 0.7f }, { envAmt, 0.65f }, { e1A, 0.0f }, { e1D, 0.4f }, { e1S, 0.0f },
                { ampA, 0.0f }, { ampD, 0.55f }, { ampS, 0.0f }, { ampR, 0.55f }, { velSens, 0.7f },
                { delaySync, 3 }, { delayFeedback, 0.4f }, { delayMix, 0.25f }, { reverbSize, 0.6f }, { reverbMix, 0.3f } },
              {} },

            { "Soft Organ", "Keys", {
                { aWave, 3 }, { aLevel, 0.5f }, { bTable, 0 }, { bPos, 0.9f }, { bLevel, 0.4f },
                { cutoff, 0.75f }, { ampA, 0.1f }, { ampD, 0.5f }, { ampS, 1.0f }, { ampR, 0.4f }, { velSens, 0.1f },
                { lfo1Rate, 0.72f }, { chorusMix, 0.5f }, { reverbSize, 0.5f }, { reverbMix, 0.25f } },
              { { Src::lfo1, Dst::pitch, 0.002f }, { Src::lfo1, Dst::pan, 0.3f } } },

            { "Celeste", "Keys", {
                { aWave, 3 }, { aTune, 1.0f }, { aLevel, 0.3f }, { aShape, 0.2f }, { bTable, 1 }, { bPos, 0.9f }, { bLevel, 0.5f },
                { cutoff, 0.85f }, { ampA, 0.0f }, { ampD, 0.65f }, { ampS, 0.05f }, { ampR, 0.6f }, { velSens, 0.6f },
                { delaySync, 4 }, { delayFeedback, 0.25f }, { delayMix, 0.12f },
                { reverbSize, 0.65f }, { reverbShimmer, 0.3f }, { reverbMix, 0.35f } },
              {} },

            { "Morning Keys", "Keys", {
                { volume, 1.0f }, { aWave, 0 }, { aShape, 0.6f }, { aDetune, 0.25f }, { aLevel, 0.55f }, { bTable, 0 }, { bPos, 0.3f }, { bLevel, 0.35f },
                { cutoff, 0.56f }, { res, 0.2f }, { envAmt, 0.66f }, { e1A, 0.0f }, { e1D, 0.55f }, { e1S, 0.2f },
                { ampA, 0.03f }, { ampD, 0.7f }, { ampS, 0.45f }, { ampR, 0.6f }, { velSens, 0.6f },
                { chorusMix, 0.4f }, { reverbSize, 0.6f }, { reverbMix, 0.3f } },
              {} },

            { "Velvet EP", "Keys", {
                { aWave, 3 }, { aShape, 0.3f }, { aLevel, 0.65f }, { aDetune, 0.15f }, { bTable, 2 }, { bPos, 0.0f }, { bLevel, 0.2f },
                { cutoff, 0.6f }, { envAmt, 0.6f }, { e1A, 0.0f }, { e1D, 0.55f }, { e1S, 0.1f },
                { ampA, 0.03f }, { ampD, 0.72f }, { ampS, 0.35f }, { ampR, 0.55f }, { velSens, 0.75f },
                { lfo1Rate, 0.66f }, { chorusMix, 0.45f }, { chorusDepth, 0.4f }, { reverbSize, 0.5f }, { reverbMix, 0.28f } },
              { { Src::lfo1, Dst::amp, 0.1f }, { Src::vel, Dst::aShape, 0.25f } } },

            { "Pressure Keys", "Keys", {
                { aWave, 0 }, { aShape, 0.4f }, { aLevel, 0.6f }, { bTable, 0 }, { bPos, 0.4f }, { bLevel, 0.4f },
                { cutoff, 0.4f }, { envAmt, 0.55f }, { ampA, 0.03f }, { ampD, 0.7f }, { ampS, 0.6f }, { ampR, 0.55f },
                { reverbSize, 0.55f }, { reverbMix, 0.3f } },
              { { Src::press, Dst::bLevel, 0.4f }, { Src::press, Dst::cutoff, 0.2f }, { Src::press, Dst::pitch, 0.01f } } },

            { "Harmonium", "Keys", {
                { aWave, 1 }, { aShape, 0.15f }, { aLevel, 0.5f }, { aDetune, 0.2f }, { bTable, 3 }, { bPos, 0.5f }, { bLevel, 0.45f },
                { air, 0.12f }, { cutoff, 0.6f }, { res, 0.1f }, { ampA, 0.4f }, { ampD, 0.5f }, { ampS, 0.9f }, { ampR, 0.45f },
                { lfo1Rate, 0.55f }, { reverbSize, 0.55f }, { reverbMix, 0.3f } },
              { { Src::lfo1, Dst::amp, 0.06f } } },

            { "Toy Box", "Keys", {
                { aWave, 1 }, { aShape, 0.5f }, { aTune, 0.75f }, { aLevel, 0.5f },
                { cutoff, 0.8f }, { ampA, 0.0f }, { ampD, 0.5f }, { ampS, 0.0f }, { ampR, 0.45f },
                { delaySync, 2 }, { delayFeedback, 0.35f }, { delayMix, 0.2f }, { reverbSize, 0.5f }, { reverbMix, 0.3f } },
              {} },

            { "Clav Bark", "Keys", {
                { aWave, 1 }, { aShape, 0.7f }, { aLevel, 0.6f }, { bTable, 3 }, { bPos, 0.8f }, { bLevel, 0.3f },
                { cutoff, 0.45f }, { res, 0.55f }, { envAmt, 0.78f }, { e1A, 0.0f }, { e1D, 0.45f }, { e1S, 0.0f },
                { ampA, 0.0f }, { ampD, 0.55f }, { ampS, 0.25f }, { ampR, 0.35f }, { velSens, 0.8f },
                { reverbSize, 0.35f }, { reverbMix, 0.15f } },
              { { Src::vel, Dst::cutoff, 0.2f } } },

            //================================================================ BELLS & PLUCKS
            { "Temple Bell", "Bells & Plucks", {
                { aWave, 3 }, { aShape, 0.7f }, { aLevel, 0.5f }, { aTune, 0.8958f }, { bTable, 1 }, { bPos, 0.7f }, { bLevel, 0.5f },
                { cutoff, 0.8f }, { ampA, 0.0f }, { ampD, 0.88f }, { ampS, 0.0f }, { ampR, 0.85f },
                { reverbSize, 0.75f }, { reverbShimmer, 0.3f }, { reverbMix, 0.45f } },
              {} },

            { "Rain Drops", "Bells & Plucks", {
                { aWave, 3 }, { aTune, 1.0f }, { aShape, 0.4f }, { aLevel, 0.5f },
                { cutoff, 0.8f }, { ampA, 0.0f }, { ampD, 0.45f }, { ampS, 0.0f }, { ampR, 0.45f },
                { arpOn, 1 }, { arpMode, 3 }, { arpRate, 3 }, { arpOctaves, 2 }, { arpGate, 0.3f },
                { delaySync, 3 }, { delayFeedback, 0.5f }, { delayMix, 0.35f },
                { reverbSize, 0.7f }, { reverbDecay, 0.7f }, { reverbMix, 0.5f } },
              { { Src::rand, Dst::pan, 0.6f } } },

            { "Bamboo", "Bells & Plucks", {
                { aWave, 2 }, { aShape, 0.6f }, { aLevel, 0.6f }, { bTable, 0 }, { bPos, 0.6f }, { bLevel, 0.3f }, { air, 0.2f },
                { cutoff, 0.55f }, { envAmt, 0.75f }, { e1A, 0.0f }, { e1D, 0.4f }, { e1S, 0.0f },
                { ampA, 0.0f }, { ampD, 0.5f }, { ampS, 0.0f }, { ampR, 0.45f }, { reverbSize, 0.5f }, { reverbMix, 0.3f } },
              {} },

            { "Crystal Harp", "Bells & Plucks", {
                { volume, 0.97f }, { aWave, 0 }, { aShape, 0.3f }, { aLevel, 0.5f }, { bTable, 1 }, { bPos, 0.4f }, { bLevel, 0.4f },
                { cutoff, 0.5f }, { res, 0.3f }, { envAmt, 0.72f }, { e1A, 0.0f }, { e1D, 0.55f }, { e1S, 0.0f },
                { ampA, 0.0f }, { ampD, 0.7f }, { ampS, 0.0f }, { ampR, 0.7f },
                { delaySync, 5 }, { delayFeedback, 0.35f }, { delayMix, 0.2f },
                { reverbSize, 0.7f }, { reverbShimmer, 0.4f }, { reverbMix, 0.45f } },
              {} },

            { "Seed Pods", "Bells & Plucks", {
                { aWave, 1 }, { aShape, 0.4f }, { aLevel, 0.55f },
                { cutoff, 0.45f }, { res, 0.6f }, { envAmt, 0.75f }, { e1A, 0.0f }, { e1D, 0.38f }, { e1S, 0.0f },
                { ampA, 0.0f }, { ampD, 0.45f }, { ampS, 0.0f }, { ampR, 0.4f },
                { arpOn, 1 }, { arpMode, 0 }, { arpRate, 3 }, { arpOctaves, 1 }, { arpGate, 0.4f }, { lfo1Rate, 0.25f },
                { delaySync, 3 }, { delayFeedback, 0.4f }, { delayMix, 0.25f }, { reverbSize, 0.55f }, { reverbMix, 0.35f } },
              { { Src::lfo1, Dst::cutoff, 0.1f } } },

            { "Singing Bowl", "Bells & Plucks", {
                { aWave, 3 }, { aShape, 0.2f }, { aLevel, 0.6f }, { bTable, 1 }, { bPos, 0.25f }, { bLevel, 0.4f }, { bUnison, 0.2f },
                { cutoff, 0.8f }, { ampA, 0.05f }, { ampD, 0.92f }, { ampS, 0.0f }, { ampR, 0.9f }, { lfo1Rate, 0.55f },
                { reverbSize, 0.8f }, { reverbDecay, 0.8f }, { reverbMix, 0.5f } },
              { { Src::lfo1, Dst::amp, 0.2f }, { Src::lfo2, Dst::pitch, 0.003f } } },

            //================================================================ TEXTURES
            { "Root System", "Textures & Bass", {
                { aWave, 0 }, { aTune, 0.25f }, { aLevel, 0.7f }, { aDetune, 0.15f }, { bTable, 0 }, { bPos, 0.2f }, { bLevel, 0.4f },
                { cutoff, 0.35f }, { res, 0.35f }, { drive, 0.3f }, { envAmt, 0.7f }, { e1A, 0.0f }, { e1D, 0.5f }, { e1S, 0.2f },
                { ampA, 0.02f }, { ampD, 0.6f }, { ampS, 0.7f }, { ampR, 0.4f }, { glide, 0.2f },
                { reverbSize, 0.3f }, { reverbMix, 0.1f } },
              {} },

            { "Firefly Lead", "Textures & Bass", {
                { aWave, 1 }, { aShape, 0.3f }, { aLevel, 0.6f }, { aDetune, 0.2f }, { glide, 0.35f },
                { cutoff, 0.65f }, { res, 0.3f }, { envAmt, 0.6f }, { ampA, 0.2f }, { ampS, 0.8f }, { ampR, 0.5f }, { lfo1Rate, 0.7f },
                { delaySync, 3 }, { delayFeedback, 0.45f }, { delayMix, 0.3f }, { reverbSize, 0.6f }, { reverbMix, 0.35f } },
              { { Src::lfo1, Dst::pitch, 0.008f }, { Src::press, Dst::pitch, 0.02f } } },

            { "Wind Through Reeds", "Textures & Bass", {
                { volume, 1.0f }, { aLevel, 0.0f }, { air, 0.7f }, { bTable, 3 }, { bPos, 0.5f }, { bLevel, 0.25f },
                { cutoff, 0.6f }, { res, 0.55f }, { ampA, 0.85f }, { ampS, 1.0f }, { ampR, 0.85f },
                { lfo1Rate, 0.2f }, { lfo2Rate, 0.4f },
                { reverbSize, 0.85f }, { reverbDecay, 0.85f }, { reverbMix, 0.55f } },
              { { Src::lfo2, Dst::cutoff, 0.25f }, { Src::lfo1, Dst::res, 0.2f } } },

            { "Mycelium", "Textures & Bass", {
                { aWave, 2 }, { aShape, 0.8f }, { aLevel, 0.5f }, { aTune, 0.25f }, { bTable, 2 }, { bPos, 0.5f }, { bLevel, 0.4f }, { bUnison, 0.6f },
                { cutoff, 0.5f }, { res, 0.4f }, { ampA, 0.8f }, { ampS, 0.95f }, { ampR, 0.85f },
                { lfo1Rate, 0.45f }, { lfo1Shape, 1 }, { lfo2Rate, 0.3f },
                { delayTime, 0.55f }, { delayFeedback, 0.55f }, { delayMix, 0.3f }, { reverbSize, 0.8f }, { reverbMix, 0.5f } },
              { { Src::lfo1, Dst::bPos, 0.5f }, { Src::lfo2, Dst::aShape, 0.3f }, { Src::rand, Dst::pan, 0.4f } } },

            { "Tidepool", "Textures & Bass", {
                { bTable, 2 }, { bPos, 0.3f }, { bLevel, 0.7f }, { bUnison, 0.4f }, { aWave, 3 }, { aLevel, 0.3f }, { aTune, 0.75f },
                { cutoff, 0.6f }, { res, 0.3f }, { ampA, 0.5f }, { ampD, 0.7f }, { ampS, 0.6f }, { ampR, 0.8f },
                { randRate, 0.55f }, { randSmooth, 0.8f },
                { delaySync, 5 }, { delayFeedback, 0.5f }, { delayMix, 0.3f }, { reverbSize, 0.85f }, { reverbShimmer, 0.3f }, { reverbMix, 0.55f } },
              { { Src::rand, Dst::bPos, 0.5f }, { Src::rand, Dst::cutoff, 0.15f } } },

            //================================================================ ETHEREAL (II)
            { "Dawn Chorus", "Ethereal", {
                { bTable, 2 }, { bPos, 0.15f }, { bUnison, 0.7f }, { bLevel, 0.7f }, { aWave, 3 }, { aTune, 0.75f }, { aLevel, 0.25f },
                { air, 0.2f }, { cutoff, 0.7f }, { ampA, 0.8f }, { ampS, 0.95f }, { ampR, 0.85f }, { lfo1Rate, 0.22f },
                { chorusMix, 0.4f }, { reverbSize, 0.85f }, { reverbDecay, 0.8f }, { reverbShimmer, 0.45f }, { reverbMix, 0.6f } },
              { { Src::lfo1, Dst::bPos, 0.45f }, { Src::lfo2, Dst::pan, 0.3f } } },

            { "Glacier Breath", "Ethereal", {
                { volume, 1.0f }, { aLevel, 0.0f }, { air, 0.7f }, { bTable, 1 }, { bPos, 0.85f }, { bLevel, 0.6f }, { bUnison, 0.5f },
                { cutoff, 0.68f }, { res, 0.35f }, { ampA, 0.85f }, { ampS, 1.0f }, { ampR, 0.9f }, { lfo2Rate, 0.25f },
                { reverbSize, 1.0f }, { reverbDecay, 0.92f }, { reverbDamp, 0.3f }, { reverbShimmer, 0.5f }, { reverbMix, 0.65f } },
              { { Src::lfo2, Dst::cutoff, 0.15f }, { Src::lfo1, Dst::air, 0.2f } } },

            { "Lotus Drift", "Ethereal", {
                { aWave, 2 }, { aShape, 0.35f }, { aLevel, 0.45f }, { aDetune, 0.25f }, { bTable, 0 }, { bPos, 0.6f }, { bLevel, 0.5f },
                { cutoff, 0.6f }, { envAmt, 0.58f }, { e1A, 0.8f }, { e1D, 0.85f }, { e1S, 0.6f },
                { ampA, 0.72f }, { ampS, 0.9f }, { ampR, 0.85f }, { lfo1Rate, 0.32f }, { glide, 0.25f },
                { delaySync, 5 }, { delayFeedback, 0.45f }, { delayMix, 0.22f }, { reverbSize, 0.8f }, { reverbDecay, 0.78f }, { reverbMix, 0.5f } },
              { { Src::lfo1, Dst::aShape, 0.25f }, { Src::lfo2, Dst::pitch, 0.004f } } },

            { "Starfield Hymn", "Ethereal", {
                { volume, 1.0f }, { bTable, 1 }, { bPos, 0.5f }, { bUnison, 1.0f }, { bWarp, 0.15f }, { bLevel, 0.7f },
                { aWave, 0 }, { aShape, 0.6f }, { aDetune, 0.5f }, { aLevel, 0.3f },
                { cutoff, 0.7f }, { ampA, 0.8f }, { ampS, 1.0f }, { ampR, 0.92f }, { lfo1Rate, 0.12f },
                { reverbSize, 1.0f }, { reverbDecay, 0.95f }, { reverbShimmer, 0.7f }, { reverbMix, 0.7f } },
              { { Src::lfo1, Dst::bPos, 0.4f } } },

            { "Mist Over Water", "Ethereal", {
                { aWave, 3 }, { aShape, 0.2f }, { aLevel, 0.55f }, { bTable, 2 }, { bPos, 0.45f }, { bLevel, 0.35f }, { air, 0.25f },
                { cutoff, 0.6f }, { ampA, 0.75f }, { ampD, 0.8f }, { ampS, 0.8f }, { ampR, 0.88f }, { lfo1Rate, 0.18f },
                { chorusMix, 0.5f }, { chorusDepth, 0.7f }, { delayTime, 0.75f }, { delayFeedback, 0.5f }, { delayTone, 0.35f }, { delayMix, 0.25f },
                { reverbSize, 0.9f }, { reverbDecay, 0.85f }, { reverbDamp, 0.65f }, { reverbMix, 0.55f } },
              { { Src::lfo1, Dst::bPos, 0.3f } } },

            { "Cedar Cathedral", "Ethereal", {
                { volume, 0.78f }, { aWave, 1 }, { aShape, 0.2f }, { aDetune, 0.2f }, { aLevel, 0.45f }, { bTable, 3 }, { bPos, 0.3f }, { bLevel, 0.4f },
                { cutoff, 0.62f }, { res, 0.15f }, { ampA, 0.6f }, { ampS, 0.95f }, { ampR, 0.85f }, { lfo1Rate, 0.55f },
                { reverbSize, 1.0f }, { reverbDecay, 0.9f }, { reverbShimmer, 0.25f }, { reverbMix, 0.6f } },
              { { Src::lfo1, Dst::amp, 0.06f } } },

            { "Snowfall", "Ethereal", {
                { volume, 0.8f }, { aWave, 3 }, { aTune, 1.0f }, { aShape, 0.4f }, { aLevel, 0.4f }, { bTable, 1 }, { bPos, 0.7f }, { bLevel, 0.4f },
                { cutoff, 0.8f }, { ampA, 0.0f }, { ampD, 0.7f }, { ampS, 0.0f }, { ampR, 0.75f },
                { arpOn, 1 }, { arpMode, 3 }, { arpRate, 4 }, { arpOctaves, 1 }, { arpGate, 0.3f },
                { delaySync, 3 }, { delayFeedback, 0.55f }, { delayMix, 0.3f },
                { reverbSize, 0.95f }, { reverbDecay, 0.85f }, { reverbShimmer, 0.55f }, { reverbMix, 0.6f } },
              { { Src::rand, Dst::pan, 0.7f }, { Src::rand, Dst::bPos, 0.3f } } },

            { "Lantern Procession", "Ethereal", {
                { aWave, 0 }, { aShape, 0.55f }, { aDetune, 0.4f }, { aLevel, 0.5f }, { bTable, 0 }, { bPos, 0.35f }, { bLevel, 0.4f },
                { cutoff, 0.5f }, { res, 0.25f }, { envAmt, 0.62f }, { e1A, 0.85f }, { e1D, 0.9f }, { e1S, 0.4f },
                { ampA, 0.82f }, { ampS, 0.95f }, { ampR, 0.88f }, { lfo2Rate, 0.18f },
                { chorusMix, 0.35f }, { reverbSize, 0.85f }, { reverbDecay, 0.8f }, { reverbMix, 0.5f } },
              { { Src::lfo2, Dst::cutoff, 0.1f } } },

            { "Echoes of Rain", "Ethereal", {
                { aWave, 2 }, { aShape, 0.5f }, { aLevel, 0.5f }, { bTable, 1 }, { bPos, 0.2f }, { bLevel, 0.35f },
                { cutoff, 0.66f }, { ampA, 0.05f }, { ampD, 0.75f }, { ampS, 0.3f }, { ampR, 0.75f },
                { delaySync, 3 }, { delayFeedback, 0.7f }, { delayTone, 0.4f }, { delayMix, 0.4f },
                { reverbSize, 0.8f }, { reverbDecay, 0.75f }, { reverbMix, 0.45f } },
              { { Src::rand, Dst::pan, 0.4f } } },

            { "Silk Road Dusk", "Ethereal", {
                { bTable, 3 }, { bPos, 0.5f }, { bWarp, 0.35f }, { bUnison, 0.4f }, { bLevel, 0.6f }, { aWave, 3 }, { aLevel, 0.3f },
                { cutoff, 0.58f }, { res, 0.3f }, { ampA, 0.6f }, { ampS, 0.9f }, { ampR, 0.8f }, { lfo1Rate, 0.4f }, { glide, 0.3f },
                { delaySync, 5 }, { delayFeedback, 0.4f }, { delayMix, 0.2f }, { reverbSize, 0.8f }, { reverbMix, 0.45f } },
              { { Src::lfo1, Dst::bWarp, 0.25f }, { Src::lfo2, Dst::pitch, 0.006f } } },

            { "Morning Fog Choir", "Ethereal", {
                { aLevel, 0.0f }, { bTable, 2 }, { bPos, 0.65f }, { bUnison, 0.85f }, { bLevel, 0.85f }, { air, 0.3f },
                { cutoff, 0.62f }, { ampA, 0.86f }, { ampS, 1.0f }, { ampR, 0.88f }, { lfo1Rate, 0.1f },
                { chorusMix, 0.5f }, { reverbSize, 0.95f }, { reverbDecay, 0.85f }, { reverbDamp, 0.6f }, { reverbMix, 0.62f } },
              { { Src::lfo1, Dst::bPos, 0.6f } } },

            { "Infinite Bloom", "Ethereal", {
                { volume, 0.95f }, { bTable, 0 }, { bPos, 0.1f }, { bUnison, 0.6f }, { bLevel, 0.75f }, { aWave, 3 }, { aLevel, 0.3f },
                { cutoff, 0.45f }, { envAmt, 0.72f }, { e2A, 0.9f }, { e2D, 0.9f }, { e2S, 1.0f }, { e1A, 0.92f }, { e1D, 0.9f }, { e1S, 1.0f },
                { ampA, 0.9f }, { ampS, 1.0f }, { ampR, 0.93f },
                { reverbSize, 1.0f }, { reverbDecay, 0.95f }, { reverbShimmer, 0.65f }, { reverbMix, 0.7f } },
              { { Src::env2, Dst::bPos, 0.7f } } },

            //================================================================ KEYS (II)
            { "Attic Upright", "Keys", {
                { aWave, 2 }, { aShape, 0.2f }, { aDetune, 0.12f }, { aLevel, 0.6f }, { bTable, 3 }, { bPos, 0.15f }, { bLevel, 0.3f },
                { cutoff, 0.48f }, { envAmt, 0.72f }, { e1A, 0.0f }, { e1D, 0.6f }, { e1S, 0.1f }, { e1R, 0.5f },
                { ampA, 0.02f }, { ampD, 0.78f }, { ampS, 0.25f }, { ampR, 0.55f }, { velSens, 0.85f },
                { reverbSize, 0.45f }, { reverbDecay, 0.45f }, { reverbDamp, 0.7f }, { reverbMix, 0.25f } },
              { { Src::vel, Dst::cutoff, 0.22f } } },

            { "Cabin EP", "Keys", {
                { aWave, 3 }, { aShape, 0.55f }, { aLevel, 0.6f }, { bTable, 1 }, { bPos, 0.05f }, { bLevel, 0.3f },
                { cutoff, 0.58f }, { envAmt, 0.62f }, { e1A, 0.0f }, { e1D, 0.5f }, { e1S, 0.15f },
                { ampA, 0.02f }, { ampD, 0.74f }, { ampS, 0.3f }, { ampR, 0.55f }, { velSens, 0.8f }, { lfo1Rate, 0.62f },
                { chorusMix, 0.3f }, { reverbSize, 0.5f }, { reverbMix, 0.28f } },
              { { Src::lfo1, Dst::pan, 0.35f }, { Src::vel, Dst::aShape, 0.3f } } },

            { "Music Box Dream", "Keys", {
                { volume, 1.0f }, { aWave, 3 }, { aTune, 1.0f }, { aShape, 0.3f }, { aLevel, 0.45f }, { bTable, 1 }, { bPos, 0.95f }, { bLevel, 0.4f },
                { cutoff, 0.85f }, { ampA, 0.0f }, { ampD, 0.62f }, { ampS, 0.0f }, { ampR, 0.6f }, { velSens, 0.5f },
                { delaySync, 4 }, { delayFeedback, 0.3f }, { delayMix, 0.15f },
                { reverbSize, 0.7f }, { reverbShimmer, 0.35f }, { reverbMix, 0.45f } },
              { { Src::lfo2, Dst::pitch, 0.003f } } },

            { "Marimba Grove", "Keys", {
                { aWave, 2 }, { aShape, 0.15f }, { aLevel, 0.7f }, { bTable, 0 }, { bPos, 0.0f }, { bLevel, 0.3f },
                { cutoff, 0.52f }, { envAmt, 0.7f }, { e1A, 0.0f }, { e1D, 0.42f }, { e1S, 0.0f },
                { ampA, 0.0f }, { ampD, 0.55f }, { ampS, 0.0f }, { ampR, 0.5f }, { velSens, 0.75f },
                { reverbSize, 0.5f }, { reverbDamp, 0.6f }, { reverbMix, 0.25f } },
              { { Src::vel, Dst::cutoff, 0.2f } } },

            { "Dulcimer Light", "Keys", {
                { volume, 1.0f }, { aWave, 0 }, { aShape, 0.35f }, { aDetune, 0.3f }, { aLevel, 0.55f }, { bTable, 3 }, { bPos, 0.7f }, { bLevel, 0.3f },
                { cutoff, 0.58f }, { res, 0.25f }, { envAmt, 0.7f }, { e1A, 0.0f }, { e1D, 0.5f }, { e1S, 0.05f },
                { ampA, 0.0f }, { ampD, 0.72f }, { ampS, 0.05f }, { ampR, 0.65f },
                { chorusMix, 0.3f }, { delaySync, 3 }, { delayFeedback, 0.3f }, { delayMix, 0.15f }, { reverbSize, 0.6f }, { reverbMix, 0.35f } },
              {} },

            { "Reed Organ Hymn", "Keys", {
                { volume, 0.78f }, { aWave, 1 }, { aShape, 0.3f }, { aLevel, 0.45f }, { aDetune, 0.15f }, { bTable, 3 }, { bPos, 0.25f }, { bLevel, 0.5f },
                { air, 0.15f }, { cutoff, 0.55f }, { ampA, 0.35f }, { ampS, 1.0f }, { ampR, 0.4f }, { velSens, 0.2f }, { lfo1Rate, 0.58f },
                { reverbSize, 0.7f }, { reverbDecay, 0.65f }, { reverbMix, 0.35f } },
              { { Src::lfo1, Dst::amp, 0.05f } } },

            { "Tine Garden", "Keys", {
                { aWave, 3 }, { aShape, 0.7f }, { aLevel, 0.55f }, { bTable, 1 }, { bPos, 0.4f }, { bLevel, 0.35f },
                { cutoff, 0.62f }, { envAmt, 0.6f }, { e1A, 0.0f }, { e1D, 0.45f }, { e1S, 0.1f },
                { ampA, 0.01f }, { ampD, 0.8f }, { ampS, 0.2f }, { ampR, 0.65f }, { velSens, 0.75f }, { lfo1Rate, 0.66f },
                { chorusMix, 0.4f }, { delaySync, 5 }, { delayFeedback, 0.35f }, { delayMix, 0.18f }, { reverbSize, 0.65f }, { reverbMix, 0.38f } },
              { { Src::lfo1, Dst::amp, 0.1f } } },

            { "Lullaby Keys", "Keys", {
                { volume, 0.92f }, { aWave, 2 }, { aShape, 0.1f }, { aLevel, 0.5f }, { bTable, 0 }, { bPos, 0.5f }, { bLevel, 0.5f }, { air, 0.08f },
                { cutoff, 0.46f }, { envAmt, 0.66f }, { e1A, 0.0f }, { e1D, 0.6f }, { e1S, 0.2f },
                { ampA, 0.05f }, { ampD, 0.8f }, { ampS, 0.4f }, { ampR, 0.7f }, { velSens, 0.7f },
                { chorusMix, 0.25f }, { reverbSize, 0.7f }, { reverbDecay, 0.65f }, { reverbDamp, 0.7f }, { reverbMix, 0.42f } },
              {} },

            { "Warm Vibes", "Keys", {
                { aWave, 3 }, { aShape, 0.05f }, { aLevel, 0.65f }, { bTable, 1 }, { bPos, 0.1f }, { bLevel, 0.2f },
                { cutoff, 0.75f }, { ampA, 0.0f }, { ampD, 0.82f }, { ampS, 0.0f }, { ampR, 0.75f }, { velSens, 0.7f }, { lfo1Rate, 0.7f },
                { reverbSize, 0.6f }, { reverbMix, 0.32f } },
              { { Src::lfo1, Dst::amp, 0.22f } } },

            { "Pocket Piano", "Keys", {
                { aWave, 1 }, { aShape, 0.45f }, { aLevel, 0.55f }, { bTable, 3 }, { bPos, 0.45f }, { bLevel, 0.25f },
                { cutoff, 0.54f }, { envAmt, 0.7f }, { e1A, 0.0f }, { e1D, 0.5f }, { e1S, 0.1f },
                { ampA, 0.0f }, { ampD, 0.7f }, { ampS, 0.2f }, { ampR, 0.5f }, { velSens, 0.75f },
                { delaySync, 2 }, { delayFeedback, 0.2f }, { delayMix, 0.1f }, { reverbSize, 0.4f }, { reverbMix, 0.2f } },
              {} },

            //================================================================ BELLS & PLUCKS (II)
            { "Wind Chimes", "Bells & Plucks", {
                { volume, 0.75f }, { aWave, 3 }, { aTune, 0.8958f }, { aShape, 0.6f }, { aLevel, 0.45f }, { bTable, 1 }, { bPos, 0.8f }, { bLevel, 0.45f },
                { cutoff, 0.85f }, { ampA, 0.0f }, { ampD, 0.9f }, { ampS, 0.0f }, { ampR, 0.88f },
                { arpOn, 1 }, { arpMode, 3 }, { arpRate, 2 }, { arpOctaves, 2 }, { arpGate, 0.2f },
                { reverbSize, 0.85f }, { reverbDecay, 0.8f }, { reverbShimmer, 0.4f }, { reverbMix, 0.55f } },
              { { Src::rand, Dst::pan, 0.8f } } },

            { "Koto Rain", "Bells & Plucks", {
                { volume, 1.0f }, { aWave, 0 }, { aShape, 0.25f }, { aLevel, 0.55f }, { bTable, 3 }, { bPos, 0.85f }, { bLevel, 0.3f },
                { cutoff, 0.48f }, { res, 0.35f }, { envAmt, 0.76f }, { e1A, 0.0f }, { e1D, 0.45f }, { e1S, 0.0f },
                { ampA, 0.0f }, { ampD, 0.66f }, { ampS, 0.0f }, { ampR, 0.6f }, { glide, 0.1f },
                { delaySync, 5 }, { delayFeedback, 0.4f }, { delayMix, 0.22f }, { reverbSize, 0.7f }, { reverbMix, 0.4f } },
              { { Src::vel, Dst::cutoff, 0.2f } } },

            { "Glass Harmonica", "Bells & Plucks", {
                { aWave, 3 }, { aLevel, 0.55f }, { bTable, 1 }, { bPos, 0.2f }, { bLevel, 0.45f },
                { cutoff, 0.8f }, { ampA, 0.55f }, { ampD, 0.85f }, { ampS, 0.7f }, { ampR, 0.8f }, { lfo1Rate, 0.6f },
                { reverbSize, 0.8f }, { reverbDecay, 0.75f }, { reverbShimmer, 0.2f }, { reverbMix, 0.5f } },
              { { Src::lfo1, Dst::amp, 0.12f }, { Src::press, Dst::bPos, 0.4f } } },

            { "Harp Steps", "Bells & Plucks", {
                { aWave, 2 }, { aShape, 0.3f }, { aLevel, 0.55f }, { bTable, 0 }, { bPos, 0.7f }, { bLevel, 0.3f },
                { cutoff, 0.55f }, { envAmt, 0.7f }, { e1A, 0.0f }, { e1D, 0.5f }, { e1S, 0.0f },
                { ampA, 0.0f }, { ampD, 0.68f }, { ampS, 0.0f }, { ampR, 0.66f },
                { arpOn, 1 }, { arpMode, 2 }, { arpRate, 3 }, { arpOctaves, 2 }, { arpGate, 0.5f },
                { delaySync, 3 }, { delayFeedback, 0.35f }, { delayMix, 0.2f }, { reverbSize, 0.75f }, { reverbMix, 0.45f } },
              {} },

            { "Raindrop Kalimba", "Bells & Plucks", {
                { aWave, 3 }, { aTune, 0.75f }, { aShape, 0.5f }, { aLevel, 0.6f }, { bTable, 1 }, { bPos, 0.5f }, { bLevel, 0.25f },
                { cutoff, 0.72f }, { envAmt, 0.62f }, { e1A, 0.0f }, { e1D, 0.38f }, { e1S, 0.0f },
                { ampA, 0.0f }, { ampD, 0.52f }, { ampS, 0.0f }, { ampR, 0.5f },
                { arpOn, 1 }, { arpMode, 3 }, { arpRate, 3 }, { arpOctaves, 1 }, { arpGate, 0.35f },
                { delaySync, 3 }, { delayFeedback, 0.5f }, { delayMix, 0.3f }, { reverbSize, 0.7f }, { reverbMix, 0.4f } },
              { { Src::rand, Dst::pan, 0.5f } } },

            { "Tibetan Bowl", "Bells & Plucks", {
                { volume, 0.8f }, { aWave, 3 }, { aShape, 0.35f }, { aLevel, 0.55f }, { aDetune, 0.08f }, { bTable, 1 }, { bPos, 0.35f }, { bLevel, 0.4f },
                { cutoff, 0.78f }, { ampA, 0.08f }, { ampD, 0.95f }, { ampS, 0.0f }, { ampR, 0.92f }, { lfo1Rate, 0.42f },
                { reverbSize, 0.9f }, { reverbDecay, 0.88f }, { reverbMix, 0.55f } },
              { { Src::lfo1, Dst::amp, 0.25f }, { Src::lfo2, Dst::pitch, 0.002f } } },

            //================================================================ TEXTURES & BASS (II)
            { "Deep Root Bass", "Textures & Bass", {
                { volume, 0.78f }, { aWave, 3 }, { aTune, 0.25f }, { aShape, 0.35f }, { aLevel, 0.75f }, { bTable, 0 }, { bPos, 0.3f }, { bLevel, 0.3f },
                { cutoff, 0.4f }, { drive, 0.25f }, { envAmt, 0.62f }, { e1A, 0.0f }, { e1D, 0.45f }, { e1S, 0.3f },
                { ampA, 0.01f }, { ampD, 0.6f }, { ampS, 0.75f }, { ampR, 0.35f }, { reverbMix, 0.08f } },
              {} },

            { "Moss Bass", "Textures & Bass", {
                { aWave, 0 }, { aTune, 0.25f }, { aShape, 0.5f }, { aDetune, 0.2f }, { aLevel, 0.65f }, { bTable, 2 }, { bPos, 0.8f }, { bLevel, 0.3f },
                { cutoff, 0.36f }, { res, 0.4f }, { drive, 0.35f }, { envAmt, 0.7f }, { e1A, 0.0f }, { e1D, 0.4f }, { e1S, 0.15f },
                { ampA, 0.01f }, { ampD, 0.6f }, { ampS, 0.6f }, { ampR, 0.35f }, { glide, 0.18f }, { reverbMix, 0.1f } },
              { { Src::press, Dst::cutoff, 0.3f } } },

            { "Breathing Earth", "Textures & Bass", {
                { volume, 0.95f }, { aWave, 0 }, { aTune, 0.25f }, { aDetune, 0.6f }, { aLevel, 0.45f }, { air, 0.4f },
                { bTable, 2 }, { bPos, 0.5f }, { bLevel, 0.35f }, { cutoff, 0.42f }, { res, 0.45f },
                { ampA, 0.85f }, { ampS, 1.0f }, { ampR, 0.9f }, { lfo1Rate, 0.12f }, { lfo1Shape, 1 }, { lfo2Rate, 0.22f },
                { reverbSize, 0.9f }, { reverbDecay, 0.85f }, { reverbMix, 0.5f } },
              { { Src::lfo1, Dst::cutoff, 0.22f }, { Src::lfo1, Dst::air, 0.25f }, { Src::lfo2, Dst::bPos, 0.3f } } },

            { "Night Insects", "Textures & Bass", {
                { aWave, 1 }, { aTune, 0.75f }, { aShape, 0.85f }, { aLevel, 0.3f }, { bTable, 1 }, { bPos, 0.9f }, { bLevel, 0.35f }, { air, 0.25f },
                { cutoff, 0.72f }, { res, 0.5f }, { ampA, 0.6f }, { ampS, 0.9f }, { ampR, 0.85f },
                { lfo1Rate, 0.78f }, { lfo1Shape, 3 }, { randRate, 0.7f }, { randSmooth, 0.2f },
                { delayTime, 0.3f }, { delayFeedback, 0.5f }, { delayMix, 0.3f }, { reverbSize, 0.85f }, { reverbMix, 0.55f } },
              { { Src::lfo1, Dst::amp, 0.4f }, { Src::rand, Dst::pan, 0.8f }, { Src::rand, Dst::bPos, 0.2f } } },

            { "Riverbed", "Textures & Bass", {
                { volume, 1.0f }, { aLevel, 0.0f }, { air, 0.6f }, { bTable, 0 }, { bPos, 0.7f }, { bLevel, 0.4f }, { bUnison, 0.7f },
                { cutoff, 0.55f }, { res, 0.55f }, { ampA, 0.85f }, { ampS, 1.0f }, { ampR, 0.88f },
                { randRate, 0.6f }, { randSmooth, 0.9f }, { lfo2Rate, 0.3f },
                { chorusMix, 0.4f }, { reverbSize, 0.9f }, { reverbDecay, 0.8f }, { reverbMix, 0.55f } },
              { { Src::rand, Dst::cutoff, 0.18f }, { Src::lfo2, Dst::bPos, 0.3f } } },

            { "Sub Hum", "Textures & Bass", {
                { aWave, 3 }, { aTune, 0.0f }, { aLevel, 0.7f }, { bTable, 3 }, { bPos, 0.1f }, { bLevel, 0.2f },
                { cutoff, 0.35f }, { drive, 0.2f }, { ampA, 0.5f }, { ampS, 1.0f }, { ampR, 0.6f }, { lfo1Rate, 0.3f },
                { reverbSize, 0.6f }, { reverbMix, 0.2f } },
              { { Src::lfo1, Dst::drive, 0.15f } } },

            //================================================================ LEADS
            { "Bamboo Flute", "Leads", {
                { aWave, 3 }, { aShape, 0.1f }, { aLevel, 0.6f }, { air, 0.35f }, { bTable, 2 }, { bPos, 0.9f }, { bLevel, 0.2f },
                { cutoff, 0.66f }, { ampA, 0.3f }, { ampS, 0.9f }, { ampR, 0.45f }, { glide, 0.2f }, { lfo1Rate, 0.66f },
                { delaySync, 3 }, { delayFeedback, 0.35f }, { delayMix, 0.18f }, { reverbSize, 0.75f }, { reverbMix, 0.4f } },
              { { Src::lfo1, Dst::pitch, 0.006f }, { Src::press, Dst::air, 0.4f }, { Src::wheel, Dst::pitch, 0.008f } } },

            { "Firefly Glide", "Leads", {
                { aWave, 2 }, { aShape, 0.45f }, { aLevel, 0.6f }, { aDetune, 0.15f }, { bTable, 1 }, { bPos, 0.6f }, { bLevel, 0.3f },
                { cutoff, 0.62f }, { res, 0.3f }, { ampA, 0.15f }, { ampS, 0.85f }, { ampR, 0.5f }, { glide, 0.45f }, { lfo1Rate, 0.7f },
                { delaySync, 5 }, { delayFeedback, 0.5f }, { delayMix, 0.3f }, { reverbSize, 0.7f }, { reverbMix, 0.38f } },
              { { Src::lfo1, Dst::pitch, 0.007f }, { Src::press, Dst::cutoff, 0.3f } } },

            { "Whistle Wind", "Leads", {
                { aWave, 3 }, { aTune, 0.75f }, { aLevel, 0.5f }, { air, 0.45f }, { bLevel, 0.0f },
                { cutoff, 0.72f }, { res, 0.2f }, { ampA, 0.35f }, { ampS, 0.85f }, { ampR, 0.55f }, { glide, 0.3f }, { lfo1Rate, 0.62f },
                { reverbSize, 0.85f }, { reverbDecay, 0.8f }, { reverbShimmer, 0.2f }, { reverbMix, 0.5f } },
              { { Src::lfo1, Dst::pitch, 0.005f }, { Src::press, Dst::air, 0.3f } } },

            { "Singing Saw", "Leads", {
                { aWave, 3 }, { aShape, 0.3f }, { aLevel, 0.6f }, { bTable, 1 }, { bPos, 0.25f }, { bLevel, 0.3f },
                { cutoff, 0.7f }, { ampA, 0.45f }, { ampS, 0.9f }, { ampR, 0.65f }, { glide, 0.6f }, { lfo1Rate, 0.6f },
                { reverbSize, 0.85f }, { reverbDecay, 0.8f }, { reverbMix, 0.5f } },
              { { Src::lfo1, Dst::pitch, 0.012f }, { Src::press, Dst::pitch, 0.015f } } },

            { "Ocarina", "Leads", {
                { aWave, 2 }, { aShape, 0.05f }, { aLevel, 0.65f }, { air, 0.18f }, { bLevel, 0.0f },
                { cutoff, 0.6f }, { ampA, 0.18f }, { ampS, 0.9f }, { ampR, 0.4f }, { glide, 0.12f }, { lfo1Rate, 0.64f },
                { reverbSize, 0.6f }, { reverbMix, 0.32f } },
              { { Src::lfo1, Dst::pitch, 0.004f }, { Src::press, Dst::cutoff, 0.25f } } },

            { "Velvet Lead", "Leads", {
                { aWave, 0 }, { aShape, 0.4f }, { aDetune, 0.3f }, { aLevel, 0.6f }, { bTable, 0 }, { bPos, 0.6f }, { bLevel, 0.35f },
                { cutoff, 0.56f }, { res, 0.25f }, { drive, 0.15f }, { envAmt, 0.62f }, { e1A, 0.1f }, { e1D, 0.6f }, { e1S, 0.5f },
                { ampA, 0.12f }, { ampS, 0.85f }, { ampR, 0.5f }, { glide, 0.25f }, { lfo1Rate, 0.66f },
                { chorusMix, 0.3f }, { delaySync, 3 }, { delayFeedback, 0.4f }, { delayMix, 0.25f }, { reverbSize, 0.65f }, { reverbMix, 0.35f } },
              { { Src::lfo1, Dst::pitch, 0.005f }, { Src::press, Dst::cutoff, 0.3f } } },

            { "Morning Bird", "Leads", {
                { aWave, 3 }, { aTune, 1.0f }, { aShape, 0.25f }, { aLevel, 0.5f }, { air, 0.15f }, { bTable, 1 }, { bPos, 0.7f }, { bLevel, 0.2f },
                { cutoff, 0.78f }, { ampA, 0.08f }, { ampD, 0.6f }, { ampS, 0.6f }, { ampR, 0.45f }, { glide, 0.35f }, { lfo1Rate, 0.78f },
                { delaySync, 1 }, { delayFeedback, 0.35f }, { delayMix, 0.2f }, { reverbSize, 0.7f }, { reverbMix, 0.4f } },
              { { Src::lfo1, Dst::pitch, 0.009f } } },
        });
        return list;
    }
}
