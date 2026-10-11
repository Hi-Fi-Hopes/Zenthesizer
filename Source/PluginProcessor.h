#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "dsp/Engine.h"
#include "dsp/Sandbox.h"
#include <memory>
#include <mutex>
#include <array>
#include <atomic>
#include <map>
#include <string>

namespace ids
{
    // Engine A / B
    inline constexpr const char* aWave = "aWave";   inline constexpr const char* aTune = "aTune";
    inline constexpr const char* aDetune = "aDetune"; inline constexpr const char* aShape = "aShape";
    inline constexpr const char* aLevel = "aLevel";
    inline constexpr const char* bTable = "bTable"; inline constexpr const char* bPos = "bPos";
    inline constexpr const char* bWarp = "bWarp";   inline constexpr const char* bUnison = "bUnison";
    inline constexpr const char* bLevel = "bLevel"; inline constexpr const char* air = "air";
    // Filter + amp
    inline constexpr const char* cutoff = "cutoff"; inline constexpr const char* res = "res";
    inline constexpr const char* drive = "drive";   inline constexpr const char* envAmt = "envAmt";
    inline constexpr const char* keyTrack = "keyTrack";
    inline constexpr const char* ampA = "ampA"; inline constexpr const char* ampD = "ampD";
    inline constexpr const char* ampS = "ampS"; inline constexpr const char* ampR = "ampR";
    // Modulators
    inline constexpr const char* e1A = "e1A"; inline constexpr const char* e1D = "e1D";
    inline constexpr const char* e1S = "e1S"; inline constexpr const char* e1R = "e1R";
    inline constexpr const char* e2A = "e2A"; inline constexpr const char* e2D = "e2D";
    inline constexpr const char* e2S = "e2S"; inline constexpr const char* e2R = "e2R";
    inline constexpr const char* lfo1Rate = "lfo1Rate"; inline constexpr const char* lfo1Shape = "lfo1Shape";
    inline constexpr const char* lfo2Rate = "lfo2Rate"; inline constexpr const char* lfo2Shape = "lfo2Shape";
    inline constexpr const char* randRate = "randRate"; inline constexpr const char* randSmooth = "randSmooth";
    // Global
    inline constexpr const char* glide = "glide";   inline constexpr const char* spread = "spread";
    inline constexpr const char* volume = "volume"; inline constexpr const char* velSens = "velSens";
    // FX
    inline constexpr const char* chorusMix = "chorusMix"; inline constexpr const char* chorusDepth = "chorusDepth";
    inline constexpr const char* chorusRate = "chorusRate";
    inline constexpr const char* delayTime = "delayTime"; inline constexpr const char* delaySync = "delaySync";
    inline constexpr const char* delayFeedback = "delayFeedback"; inline constexpr const char* delayTone = "delayTone";
    inline constexpr const char* delayMix = "delayMix";
    inline constexpr const char* reverbSize = "reverbSize"; inline constexpr const char* reverbDecay = "reverbDecay";
    inline constexpr const char* reverbDamp = "reverbDamp"; inline constexpr const char* reverbShimmer = "reverbShimmer";
    inline constexpr const char* reverbMix = "reverbMix";
    // Play
    inline constexpr const char* root = "root"; inline constexpr const char* scale = "scale";
    inline constexpr const char* octave = "octave"; inline constexpr const char* latchTimbre = "latchTimbre";
    inline constexpr const char* padChannel = "padChannel"; inline constexpr const char* platesFromPads = "platesFromPads";
    inline constexpr const char* arpOn = "arpOn"; inline constexpr const char* arpMode = "arpMode";
    inline constexpr const char* arpRate = "arpRate"; inline constexpr const char* arpOctaves = "arpOctaves";
    inline constexpr const char* arpGate = "arpGate"; inline constexpr const char* arpHold = "arpHold";

    inline juce::String slotSrc (int i) { return "slot" + juce::String (i + 1) + "Src"; }
    inline juce::String slotDst (int i) { return "slot" + juce::String (i + 1) + "Dst"; }
    inline juce::String slotAmt (int i) { return "slot" + juce::String (i + 1) + "Amt"; }
}

class ZenProcessor : public juce::AudioProcessor
{
public:
    ZenProcessor();
    ~ZenProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Zenthesizer"; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 20.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentPreset; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    static juce::StringArray waveNames()     { return { "Saw", "Square", "Triangle", "Sine" }; }
    static juce::StringArray tableNames()    { return { "Air", "Glass", "Vox", "Reed", "Own" }; }
    static juce::StringArray lfoShapeNames() { return { "Sine", "Triangle", "Saw", "Square", "Drift" }; }
    static juce::StringArray srcNames();
    static juce::StringArray srcShortNames();
    static juce::StringArray dstNames();
    static juce::StringArray scaleNames();
    static juce::StringArray rootNames()     { return { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }; }
    static juce::StringArray arpModeNames()  { return { "Up", "Down", "Up-Down", "Random", "As Played" }; }
    static juce::StringArray arpRateNames()  { return { "1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32" }; }
    static juce::StringArray delaySyncNames(){ return { "Free", "1/16", "1/8", "1/8D", "1/4", "1/4D", "1/2" }; }

    struct PresetInfo { juce::String name, category; };
    static std::vector<PresetInfo> presetInfo();

    // Plates: mouse presses from the UI, and pad learning.
    void setUiPlate (int plate, float pressure) { engine.setUiPlate (plate, pressure); }
    int  getPadNote (int plate) const           { return padNotes[(size_t) plate].load(); }
    void setPadNote (int plate, int note);
    void resetPadNotes();

    // The looper (after the whole synth, so preset changes don't touch it)
    zen::Looper& looper()                       { return engine.looper; }
    std::atomic<float> loopLevel { 0.8f };

    // Visuals: 0 = off, 1 = low, 2 = full (saved with the session, not automatable)
    std::atomic<int> visualsMode { 2 };

    // Mod quick-assign: find (or make) the slot for src -> dst and return its index, -1 if none free.
    int findOrMakeSlot (zen::Src src, zen::Dst dst);
    int findSlot (zen::Src src, zen::Dst dst) const;

    const zen::Display& display() const { return engine.display; }

    //==========================================================================
    // The wavetable sandbox: Engine B's own table (OWN), made of 1..8 keyframes.
    using Keyframes = std::vector<zen::sandbox::Cycle>;
    Keyframes getKeyframes() const;
    void setKeyframes (Keyframes);                       // rebuilds the table in the background
    int keyframesVersion() const                         { return kfVersion.load(); }
    std::shared_ptr<const zen::UserTable> getUserTable() const { return std::atomic_load (&userTable); }

    // Import any audio file the device can read (WAV, AIFF, FLAC...). Replaces the keyframes.
    bool importAudio (std::unique_ptr<juce::InputStream>, juce::String& message);
    bool exportWavetable (const juce::File&) const;      // 32-bit float WAV, 2048 samples per keyframe

    static juce::File dataFolder();                      // .../Zenthesizer
    static juce::File wavetableFolder()                  { return dataFolder().getChildFile ("Wavetables"); }
    static juce::File presetFolder()                     { return dataFolder().getChildFile ("Presets"); }

    // User presets: everything (sound + your waves), saved as a file you can reload anywhere.
    bool saveUserPreset (const juce::String& name);
    bool loadUserPreset (const juce::File&);
    static juce::Array<juce::File> userPresets();
    juce::String currentPresetName() const;
    std::atomic<float> hostBpm { 0.0f };

    juce::AudioProcessorValueTreeState apvts;

private:
    zen::Settings readSettings() const;
    float v (const char* id) const;
    int   iv (const char* id) const { return (int) std::lround (v (id)); }
    bool  bv (const char* id) const { return v (id) > 0.5f; }

    zen::Engine engine;
    std::vector<zen::MidiEvent> events;

    // sandbox
    class TableBuilder;
    mutable std::mutex kfLock;
    Keyframes keyframes;
    std::atomic<int> kfVersion { 0 };
    std::shared_ptr<const zen::UserTable> userTable;
    std::mutex retireLock;
    std::vector<std::pair<juce::uint32, std::shared_ptr<const zen::UserTable>>> retired;
    std::unique_ptr<TableBuilder> builder;
    void publishTable (zen::UserTable);
    void applyState (const juce::ValueTree&, bool includePlaySettings);
    juce::String userPresetName;
    std::array<std::atomic<int>, zen::numPlates> padNotes {};
    int currentPreset = 0;
    std::map<std::string, std::atomic<float>*> raw;
    std::array<std::atomic<float>*, zen::numSlots> slotSrcP {}, slotDstP {}, slotAmtP {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ZenProcessor)
};
