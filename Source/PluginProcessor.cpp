#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets.h"

namespace
{
    using Attr = juce::AudioParameterFloatAttributes;

    Attr percent()
    {
        return Attr().withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + "%"; });
    }
    Attr timeAttr()
    {
        return Attr().withStringFromValueFunction ([] (float v, int)
        {
            const float s = zen::envTimeSec (v);
            return s < 1.0f ? juce::String (juce::roundToInt (s * 1000.0f)) + " ms" : juce::String (s, 2) + " s";
        });
    }
}

//==============================================================================
juce::StringArray ZenProcessor::srcNames()
{
    return { "None", "Env 1", "Env 2", "LFO 1", "LFO 2", "Random", "Pressure", "Velocity", "Mod Wheel", "Channel Pressure",
             "Plate A", "Plate B", "Plate C", "Plate D", "Key", "Slide (MPE)" };
}
juce::StringArray ZenProcessor::srcShortNames()
{
    return { "-", "ENV1", "ENV2", "LFO1", "LFO2", "RAND", "PRESS", "VEL", "WHEEL", "AT", "A", "B", "C", "D", "KEY", "SLIDE" };
}
juce::StringArray ZenProcessor::dstNames()
{
    return { "None", "Pitch", "A Tune", "A Detune", "A Shape", "A Level", "B Position", "B Warp", "B Unison", "B Level", "Air",
             "Cutoff", "Resonance", "Drive", "Amp", "Pan", "LFO 1 Rate", "LFO 2 Rate", "Chorus Mix", "Delay Mix", "Reverb Mix" };
}
juce::StringArray ZenProcessor::scaleNames()
{
    juce::StringArray s;
    for (int i = 0; i < zen::numScales; ++i) s.add (zen::scaleAt (i).name);
    return s;
}

juce::AudioProcessorValueTreeState::ParameterLayout ZenProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto knob = [&] (const char* id, const char* name, float def, Attr a = percent())
    {
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (0.0f, 1.0f), def, a));
    };
    auto choice = [&] (const char* id, const char* name, const StringArray& items, int def)
    {
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, items, def));
    };
    auto toggle = [&] (const char* id, const char* name, bool def)
    {
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def));
    };

    const zen::Settings d;

    choice (ids::aWave, "A Wave", waveNames(), 0);
    knob (ids::aTune, "A Tune", 0.5f, Attr().withStringFromValueFunction ([] (float v, int)
    {
        const int st = (int) zen::tuneSemisSnapped (v);
        return (st > 0 ? "+" : "") + String (st) + " st";
    }));
    knob (ids::aDetune, "A Detune", d.aDetune);
    knob (ids::aShape, "A Shape", d.aShape);
    knob (ids::aLevel, "A Level", d.aLevel);

    choice (ids::bTable, "B Table", tableNames(), 0);
    knob (ids::bPos, "B Position", d.bPos);
    knob (ids::bWarp, "B Warp", d.bWarp);
    knob (ids::bUnison, "B Unison", d.bUnison);
    knob (ids::bLevel, "B Level", d.bLevel);
    knob (ids::air, "Air", d.air);

    knob (ids::cutoff, "Cutoff", d.cutoff, Attr().withStringFromValueFunction ([] (float v, int)
    {
        const float hz = zen::cutoffHz (v);
        return hz < 1000.0f ? String (roundToInt (hz)) + " Hz" : String (hz / 1000.0f, 1) + " kHz";
    }));
    knob (ids::res, "Resonance", d.res);
    knob (ids::drive, "Drive", d.drive);
    knob (ids::envAmt, "Filter Env", d.envAmt, Attr().withStringFromValueFunction ([] (float v, int)
    {
        const int p = roundToInt (zen::bipolar (v) * 100.0f);
        return (p > 0 ? "+" : "") + String (p) + "%";
    }));
    knob (ids::keyTrack, "Key Track", d.keyTrack);
    knob (ids::ampA, "Attack", d.ampA, timeAttr());
    knob (ids::ampD, "Decay", d.ampD, timeAttr());
    knob (ids::ampS, "Sustain", d.ampS);
    knob (ids::ampR, "Release", d.ampR, timeAttr());

    knob (ids::e1A, "Env 1 Attack", d.e1A, timeAttr());  knob (ids::e1D, "Env 1 Decay", d.e1D, timeAttr());
    knob (ids::e1S, "Env 1 Sustain", d.e1S);             knob (ids::e1R, "Env 1 Release", d.e1R, timeAttr());
    knob (ids::e2A, "Env 2 Attack", d.e2A, timeAttr());  knob (ids::e2D, "Env 2 Decay", d.e2D, timeAttr());
    knob (ids::e2S, "Env 2 Sustain", d.e2S);             knob (ids::e2R, "Env 2 Release", d.e2R, timeAttr());

    auto hzAttr = Attr().withStringFromValueFunction ([] (float v, int)
    {
        const float hz = zen::lfoHz (v);
        return hz < 1.0f ? String (hz, 2) + " Hz" : String (hz, 1) + " Hz";
    });
    knob (ids::lfo1Rate, "LFO 1 Rate", d.lfo1Rate, hzAttr);
    choice (ids::lfo1Shape, "LFO 1 Shape", lfoShapeNames(), 0);
    knob (ids::lfo2Rate, "LFO 2 Rate", d.lfo2Rate, hzAttr);
    choice (ids::lfo2Shape, "LFO 2 Shape", lfoShapeNames(), 4);
    knob (ids::randRate, "Random Rate", d.randRate, hzAttr);
    knob (ids::randSmooth, "Random Smooth", d.randSmooth);

    for (int i = 0; i < zen::numSlots; ++i)
    {
        const String n = "Mod " + String (i + 1);
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ids::slotSrc (i), 1 }, n + " Source", srcNames(), 0));
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ids::slotDst (i), 1 }, n + " Target", dstNames(), 0));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::slotAmt (i), 1 }, n + " Amount",
                        NormalisableRange<float> (-1.0f, 1.0f), 0.0f,
                        Attr().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v * 100.0f)) + "%"; })));
    }

    knob (ids::glide, "Glide", d.glide);
    knob (ids::spread, "Spread", d.spread);
    knob (ids::volume, "Volume", d.volume, Attr().withStringFromValueFunction ([] (float v, int) { return String (zen::volumeDb (v), 1) + " dB"; }));
    knob (ids::velSens, "Velocity", d.velSens);

    knob (ids::chorusMix, "Chorus Mix", d.chorusMix);
    knob (ids::chorusDepth, "Chorus Depth", d.chorusDepth);
    knob (ids::chorusRate, "Chorus Rate", d.chorusRate);
    knob (ids::delayTime, "Delay Time", d.delayTime, Attr().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (zen::delayMs (v))) + " ms"; }));
    choice (ids::delaySync, "Delay Sync", delaySyncNames(), 0);
    knob (ids::delayFeedback, "Delay Feedback", d.delayFeedback);
    knob (ids::delayTone, "Delay Tone", d.delayTone);
    knob (ids::delayMix, "Delay Mix", d.delayMix);
    knob (ids::reverbSize, "Reverb Size", d.reverbSize);
    knob (ids::reverbDecay, "Reverb Decay", d.reverbDecay, Attr().withStringFromValueFunction ([] (float v, int) { return String (zen::reverbSec (v), 1) + " s"; }));
    knob (ids::reverbDamp, "Reverb Damping", d.reverbDamp);
    knob (ids::reverbShimmer, "Shimmer", d.reverbShimmer);
    knob (ids::reverbMix, "Reverb Mix", d.reverbMix);

    choice (ids::root, "Root", rootNames(), 0);
    choice (ids::scale, "Scale", scaleNames(), 6);
    choice (ids::octave, "Octave", { "-3", "-2", "-1", "0", "+1", "+2", "+3" }, 3);
    toggle (ids::latchTimbre, "Latch Timbre Plates", false);
    {
        StringArray chans { "Any" };
        for (int c = 1; c <= 16; ++c) chans.add (String (c));
        choice (ids::padChannel, "Pad Channel", chans, 0);
    }
    toggle (ids::platesFromPads, "Pads Play Plates", true);
    toggle (ids::arpOn, "Arp On", false);
    choice (ids::arpMode, "Arp Mode", arpModeNames(), 0);
    choice (ids::arpRate, "Arp Rate", arpRateNames(), 3);
    choice (ids::arpOctaves, "Arp Octaves", { "1", "2", "3", "4" }, 0);
    knob (ids::arpGate, "Arp Gate", d.arpGate);
    toggle (ids::arpHold, "Arp Hold", false);
    choice (ids::bendRange, "Bend Range", { "1", "2", "3", "4", "5", "7", "12", "24" }, 1);
    choice (ids::mpe, "MPE", { "Off", "On, bend 12", "On, bend 24", "On, bend 48", "On, bend 96" }, 0);

    return layout;
}

//==============================================================================
//==============================================================================
// Builds the user's table off the audio thread; always works on the newest request.
class ZenProcessor::TableBuilder : public juce::Thread
{
public:
    explicit TableBuilder (ZenProcessor& p) : juce::Thread ("Zenthesizer table builder"), owner (p) { startThread(); }
    ~TableBuilder() override { signalThreadShouldExit(); notify(); stopThread (4000); }

    void request (Keyframes k)
    {
        {
            const std::lock_guard<std::mutex> l (lock);
            pending = std::move (k);
            hasPending = true;
        }
        notify();
    }

    void run() override
    {
        while (! threadShouldExit())
        {
            Keyframes job;
            bool got = false;
            {
                const std::lock_guard<std::mutex> l (lock);
                if (hasPending) { job = std::move (pending); hasPending = false; got = true; }
            }
            if (got) owner.publishTable (zen::UserTable::build (job));
            else wait (-1);
        }
    }

private:
    ZenProcessor& owner;
    std::mutex lock;
    Keyframes pending;
    bool hasPending = false;
};

ZenProcessor::~ZenProcessor()
{
    builder.reset();
    engine.setUserTable (nullptr);
}

ZenProcessor::ZenProcessor (bool lightweightCopy)
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "ZenthesizerState", createLayout()),
      lightweight (lightweightCopy)
{
    for (auto* param : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (param))
            raw[rp->getParameterID().toStdString()] = apvts.getRawParameterValue (rp->getParameterID());

    for (int i = 0; i < zen::numSlots; ++i)
    {
        slotSrcP[(size_t) i] = apvts.getRawParameterValue (ids::slotSrc (i));
        slotDstP[(size_t) i] = apvts.getRawParameterValue (ids::slotDst (i));
        slotAmtP[(size_t) i] = apvts.getRawParameterValue (ids::slotAmt (i));
    }
    resetPadNotes();

    // the sandbox starts with a gentle set of keyframes to morph through
    {
        using namespace zen::sandbox;
        keyframes = { make (Shape::sine), make (Shape::organ), make (Shape::hollow), make (Shape::bell) };
        publishTable (zen::UserTable::build (keyframes));
    }
    if (! lightweight) builder = std::make_unique<TableBuilder> (*this);   // a capture copy builds tables right away

    zen::applyGroove (drumSettings, zen::grooves()[1]);   // a groove ready to go (not playing)
    drumSettings.playing = false;
    ++drumVersion;

    setCurrentProgram (1);   // open on the first real preset...
    if (! lightweight && openingFile().existsAsFile())
        loadUserPreset (openingFile());   // ...or the opening sound you picked
}

//==============================================================================
zen::DrumSettings ZenProcessor::getDrums() const
{
    const std::lock_guard<std::mutex> l (drumLock);
    return drumSettings;
}

void ZenProcessor::setDrums (const zen::DrumSettings& d)
{
    {
        const std::lock_guard<std::mutex> l (drumLock);
        drumSettings = d;
    }
    ++drumVersion;
}

void ZenProcessor::setDrumStep (int drum, int step, int value)
{
    if (! juce::isPositiveAndBelow (drum, zen::numDrums) || ! juce::isPositiveAndBelow (step, zen::numSteps)) return;
    {
        const std::lock_guard<std::mutex> l (drumLock);
        drumSettings.steps[(size_t) drum][(size_t) step] = (uint8_t) juce::jlimit (0, 2, value);
    }
    ++drumVersion;
}

void ZenProcessor::setDrumSound (int row, int sound)
{
    if (! juce::isPositiveAndBelow (row, zen::numDrums)) return;
    {
        const std::lock_guard<std::mutex> l (drumLock);
        drumSettings.sound[(size_t) row] = sound;
    }
    if (sound != (int) zen::Sound::patch)
    {
        patchRecipes[(size_t) row] = {};
        ++captureGen[(size_t) row];   // a capture still running for this row is now out of date
    }
    ++drumVersion;
}

void ZenProcessor::loadKit (int index, bool keepPatchRows)
{
    if (! juce::isPositiveAndBelow (index, (int) zen::kits().size())) return;
    const auto before = getDrums();
    for (int r = 0; r < zen::numDrums; ++r)
    {
        if (keepPatchRows && before.sound[(size_t) r] == (int) zen::Sound::patch) continue;
        setDrumSound (r, zen::kits()[(size_t) index].sounds[(size_t) r]);
        const std::lock_guard<std::mutex> l (drumLock);
        drumSettings.tune[(size_t) r] = 0.5f;
        drumSettings.decay[(size_t) r] = 0.5f;
    }
    ++drumVersion;
}

juce::String ZenProcessor::drumRowName (int row) const
{
    if (! juce::isPositiveAndBelow (row, zen::numDrums)) return {};
    const auto d = getDrums();
    if (d.sound[(size_t) row] == (int) zen::Sound::patch)
    {
        auto n = patchRecipes[(size_t) row].name.toUpperCase();
        return n.length() > 12 ? n.substring (0, 11) + "." : n;
    }
    return zen::soundName (d.sound[(size_t) row]);
}

ZenProcessor::PatchRecipe ZenProcessor::snapshotOfCurrentSound()
{
    PatchRecipe r;
    r.kind = 3;
    r.name = currentPresetName();
    snapshotting = true;               // leave the grid's own captured patches out (no snapshots inside snapshots)
    getStateInformation (r.state);
    snapshotting = false;
    return r;
}

void ZenProcessor::captureDrumPatch (int row, PatchRecipe recipe)
{
    if (! juce::isPositiveAndBelow (row, zen::numDrums) || recipe.kind == 0) return;
    patchRecipes[(size_t) row] = recipe;
    {
        const std::lock_guard<std::mutex> l (drumLock);
        drumSettings.sound[(size_t) row] = (int) zen::Sound::patch;
    }
    ++drumVersion;
    recaptureDrumPatch (row);
}

void ZenProcessor::recaptureDrumPatch (int row)
{
    if (! juce::isPositiveAndBelow (row, zen::numDrums)) return;
    const auto recipe = patchRecipes[(size_t) row];
    if (recipe.kind == 0) return;
    const int note = noteForTune (getDrums().tune[(size_t) row]);
    const int gen = ++captureGen[(size_t) row];

    // a throwaway copy of the synth, set up with that sound (made here, on the message thread)
    auto copy = std::make_shared<ZenProcessor> (true);
    if (recipe.kind == 1)
    {
        const auto info = presetInfo();
        for (int i = 0; i < (int) info.size(); ++i)
            if (info[(size_t) i].name == recipe.name) { copy->setCurrentProgram (i); break; }
    }
    else if (recipe.kind == 2) copy->loadUserPreset (juce::File (recipe.file));
    else copy->setStateInformation (recipe.state.getData(), (int) recipe.state.getSize());
    {
        auto d = copy->getDrums(); d.playing = false; copy->setDrums (d);   // just the synth
    }

    const double fs = getSampleRate() > 0.0 ? getSampleRate() : 48000.0;
    ++capturesRunning;
    juce::Thread::launch ([copy, fs, note, row, gen, alive = std::weak_ptr<int> (lifeToken), this]
    {
        // play the note once, offline: hold ~2 s, then let it ring out, 3 s at most
        copy->setPlayConfigDetails (0, 2, fs, 512);
        copy->prepareToPlay (fs, 512);
        const int total = (int) (fs * 3.0), hold = (int) (fs * 2.0);
        auto smp = std::make_shared<zen::DrumSample>();
        smp->L.reserve ((size_t) total); smp->R.reserve ((size_t) total);
        juce::AudioBuffer<float> buf (2, 512);
        for (int pos = 0; pos < total; pos += 512)
        {
            juce::MidiBuffer midi;
            if (pos == 0) midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 110), 0);
            if (hold >= pos && hold < pos + 512) midi.addEvent (juce::MidiMessage::noteOff (1, note), hold - pos);
            copy->processBlock (buf, midi);
            const int n = std::min (512, total - pos);
            smp->L.insert (smp->L.end(), buf.getReadPointer (0), buf.getReadPointer (0) + n);
            smp->R.insert (smp->R.end(), buf.getReadPointer (1), buf.getReadPointer (1) + n);
        }
        // trim the silence at the end, fade the last 20 ms, bring the peak to a steady level
        int last = 0; float peak = 0.0f;
        for (int i = 0; i < (int) smp->L.size(); ++i)
        {
            const float a = std::max (std::abs (smp->L[(size_t) i]), std::abs (smp->R[(size_t) i]));
            if (a > 1.0e-4f) last = i;
            peak = std::max (peak, a);
        }
        const int len = std::min ((int) smp->L.size(), last + (int) (fs * 0.05));
        smp->L.resize ((size_t) std::max (16, len)); smp->R.resize ((size_t) std::max (16, len));
        const int fade = std::min ((int) smp->L.size() / 2, (int) (fs * 0.02));
        for (int i = 0; i < fade; ++i)
        {
            const float g = (float) i / (float) fade;
            smp->L[smp->L.size() - 1 - (size_t) i] *= g; smp->R[smp->R.size() - 1 - (size_t) i] *= g;
        }
        if (peak > 1.0e-4f) { const float g = 0.8f / peak; for (auto& v : smp->L) v *= g; for (auto& v : smp->R) v *= g; }

        juce::MessageManager::callAsync ([copy, alive, this, row, gen, smp]() mutable
        {
            if (alive.lock() == nullptr) return;   // the plugin has gone
            --capturesRunning;
            installPatchSample (row, gen, std::move (smp));
            copy.reset();
        });
    });
}

void ZenProcessor::installPatchSample (int row, int gen, std::shared_ptr<const zen::DrumSample> smp)
{
    if (gen != captureGen[(size_t) row]) return;   // something newer was asked for
    auto old = patchSamples[(size_t) row];
    patchSamples[(size_t) row] = smp;
    engine.drums.setSample (row, smp.get());
    // a voice may still be playing the old one for a few seconds
    const auto now = juce::Time::getMillisecondCounter();
    if (old != nullptr) retiredSamples.push_back ({ now, std::move (old) });
    retiredSamples.erase (std::remove_if (retiredSamples.begin(), retiredSamples.end(),
                                          [now] (const auto& r) { return now - r.first > 10000; }), retiredSamples.end());
}

void ZenProcessor::loadGroove (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) zen::grooves().size())) return;
    {
        const std::lock_guard<std::mutex> l (drumLock);
        zen::applyGroove (drumSettings, zen::grooves()[(size_t) index]);
    }
    ++drumVersion;
}

//==============================================================================
void ZenProcessor::publishTable (zen::UserTable t)
{
    auto fresh = std::make_shared<const zen::UserTable> (std::move (t));
    auto old = std::atomic_exchange (&userTable, fresh);
    engine.setUserTable (fresh.get());

    // The audio thread may still be reading the old one for the rest of its block:
    // keep it alive for two seconds before letting it go.
    const auto now = juce::Time::getMillisecondCounter();
    const std::lock_guard<std::mutex> l (retireLock);
    if (old != nullptr) retired.push_back ({ now, std::move (old) });
    retired.erase (std::remove_if (retired.begin(), retired.end(),
                                   [now] (const auto& r) { return now - r.first > 2000; }), retired.end());
}

ZenProcessor::Keyframes ZenProcessor::getKeyframes() const
{
    const std::lock_guard<std::mutex> l (kfLock);
    return keyframes;
}

void ZenProcessor::setKeyframes (Keyframes k)
{
    if (k.empty()) return;
    if ((int) k.size() > zen::maxKeyframes) k.resize ((size_t) zen::maxKeyframes);
    for (auto& c : k) c.resize ((size_t) zen::tableSize, 0.0f);
    {
        const std::lock_guard<std::mutex> l (kfLock);
        keyframes = k;
    }
    ++kfVersion;
    if (builder != nullptr) builder->request (std::move (k));
    else publishTable (zen::UserTable::build (k));
}

bool ZenProcessor::importAudio (std::unique_ptr<juce::InputStream> in, juce::String& message)
{
    if (in == nullptr) { message = "couldn't open that file"; return false; }
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (std::move (in)));
    if (reader == nullptr) { message = "that file isn't audio the app can read (try WAV)"; return false; }

    const int len = (int) std::min<juce::int64> (reader->lengthInSamples, (juce::int64) reader->sampleRate * 30);
    if (len < 8) { message = "that file is empty"; return false; }
    juce::AudioBuffer<float> buf ((int) std::max (1u, reader->numChannels), len);
    reader->read (&buf, 0, len, 0, true, true);
    std::vector<float> mono ((size_t) len, 0.0f);
    for (int c = 0; c < buf.getNumChannels(); ++c)
        for (int i = 0; i < len; ++i) mono[(size_t) i] += buf.getSample (c, i) / (float) buf.getNumChannels();

    auto k = zen::sandbox::keyframesFromAudio (mono);
    if (k.empty()) { message = "couldn't find a usable waveform in that file"; return false; }
    message = juce::String ((int) k.size()) + (k.size() == 1 ? " keyframe" : " keyframes") + " imported";
    setKeyframes (std::move (k));
    return true;
}

bool ZenProcessor::exportWavetable (const juce::File& f) const
{
    const auto k = getKeyframes();
    juce::AudioBuffer<float> buf (1, (int) k.size() * zen::tableSize);
    for (size_t i = 0; i < k.size(); ++i)
        buf.copyFrom (0, (int) i * zen::tableSize, k[i].data(), zen::tableSize);

    f.getParentDirectory().createDirectory();
    f.deleteFile();
    std::unique_ptr<juce::OutputStream> os (new juce::FileOutputStream (f));
    if (! static_cast<juce::FileOutputStream*> (os.get())->openedOk()) return false;
    juce::WavAudioFormat wav;
    auto writer = wav.createWriterFor (os, juce::AudioFormatWriterOptions{}.withSampleRate (48000.0).withNumChannels (1)
                                                                          .withBitsPerSample (32)
                                                                          .withSampleFormat (juce::AudioFormatWriterOptions::SampleFormat::floatingPoint));
    if (writer == nullptr) return false;
    return writer->writeFromAudioSampleBuffer (buf, 0, buf.getNumSamples());
}

juce::File ZenProcessor::dataFolder()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("Zenthesizer");
}

juce::Array<juce::File> ZenProcessor::userPresets()
{
    auto files = presetFolder().findChildFiles (juce::File::findFiles, false, "*.zenpreset");
    files.sort();
    return files;
}

juce::String ZenProcessor::currentPresetName() const
{
    if (userPresetName.isNotEmpty()) return userPresetName;
    const auto info = presetInfo();
    return juce::isPositiveAndBelow (currentPreset, (int) info.size()) ? info[(size_t) currentPreset].name : juce::String ("Init");
}

bool ZenProcessor::saveUserPreset (const juce::String& name)
{
    const auto clean = juce::File::createLegalFileName (name.trim());
    if (clean.isEmpty()) return false;
    juce::MemoryBlock mb;
    getStateInformation (mb);
    auto xml = getXmlFromBinary (mb.getData(), (int) mb.getSize());
    if (xml == nullptr) return false;
    xml->setAttribute ("presetName", name.trim());
    presetFolder().createDirectory();
    if (! xml->writeTo (presetFolder().getChildFile (clean + ".zenpreset"))) return false;
    userPresetName = name.trim();
    return true;
}

bool ZenProcessor::setOpeningSound()
{
    juce::MemoryBlock mb;
    snapshotting = true;   // no captured drum patches in it
    getStateInformation (mb);
    snapshotting = false;
    auto xml = getXmlFromBinary (mb.getData(), (int) mb.getSize());
    if (xml == nullptr) return false;
    xml->setAttribute ("presetName", currentPresetName());
    dataFolder().createDirectory();
    return xml->writeTo (openingFile());
}

juce::String ZenProcessor::openingSoundName()
{
    if (! openingFile().existsAsFile()) return {};
    if (auto xml = juce::XmlDocument::parse (openingFile())) return xml->getStringAttribute ("presetName", "your sound");
    return {};
}

bool ZenProcessor::loadUserPreset (const juce::File& f)
{
    auto xml = juce::XmlDocument::parse (f);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return false;
    applyState (juce::ValueTree::fromXml (*xml), false);
    userPresetName = xml->getStringAttribute ("presetName", f.getFileNameWithoutExtension());
    return true;
}

// Applies a saved state. includePlaySettings = false keeps your scale, pads, etc. (loading a preset).
void ZenProcessor::applyState (const juce::ValueTree& tree, bool includePlaySettings)
{
    static const juce::StringArray playIds { ids::root, ids::scale, ids::octave, ids::padChannel, ids::platesFromPads, ids::latchTimbre, ids::bendRange, ids::mpe };
    if (includePlaySettings)
    {
        apvts.replaceState (tree);
    }
    else
    {
        for (int i = 0; i < tree.getNumChildren(); ++i)
        {
            const auto child = tree.getChild (i);
            const auto id = child.getProperty ("id").toString();
            if (id.isEmpty() || playIds.contains (id)) continue;
            if (auto* prm = apvts.getParameter (id))
            {
                prm->beginChangeGesture();
                prm->setValueNotifyingHost (prm->convertTo0to1 ((float) (double) child.getProperty ("value")));
                prm->endChangeGesture();
            }
        }
    }

    // the sandbox keyframes
    const auto frames = (int) tree.getProperty ("ownFrames", 0);
    juce::MemoryBlock mb;
    if (frames > 0 && mb.fromBase64Encoding (tree.getProperty ("ownWaves", "").toString())
        && mb.getSize() == (size_t) frames * zen::tableSize * sizeof (float))
    {
        Keyframes k ((size_t) frames, zen::sandbox::blank());
        const auto* src = static_cast<const float*> (mb.getData());
        for (int f = 0; f < frames; ++f) std::copy (src + f * zen::tableSize, src + (f + 1) * zen::tableSize, k[(size_t) f].begin());
        setKeyframes (std::move (k));
    }
}

float ZenProcessor::v (const char* id) const
{
    auto it = raw.find (id);
    jassert (it != raw.end());
    return it != raw.end() ? it->second->load() : 0.0f;
}

bool ZenProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void ZenProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, lightweight);
    events.reserve ((size_t) std::max (256, samplesPerBlock));
}

zen::Settings ZenProcessor::readSettings() const
{
    zen::Settings s;
    s.aWave = (zen::WaveA) juce::jlimit (0, 4, iv (ids::aWave));
    s.aTune = v (ids::aTune); s.aDetune = v (ids::aDetune); s.aShape = v (ids::aShape); s.aLevel = v (ids::aLevel);
    s.bTable = juce::jlimit (0, 3, iv (ids::bTable));
    s.bPos = v (ids::bPos); s.bWarp = v (ids::bWarp); s.bUnison = v (ids::bUnison); s.bLevel = v (ids::bLevel);
    s.air = v (ids::air);
    s.cutoff = v (ids::cutoff); s.res = v (ids::res); s.drive = v (ids::drive); s.envAmt = v (ids::envAmt); s.keyTrack = v (ids::keyTrack);
    s.ampA = v (ids::ampA); s.ampD = v (ids::ampD); s.ampS = v (ids::ampS); s.ampR = v (ids::ampR);
    s.e1A = v (ids::e1A); s.e1D = v (ids::e1D); s.e1S = v (ids::e1S); s.e1R = v (ids::e1R);
    s.e2A = v (ids::e2A); s.e2D = v (ids::e2D); s.e2S = v (ids::e2S); s.e2R = v (ids::e2R);
    s.lfo1Rate = v (ids::lfo1Rate); s.lfo1Shape = (zen::LfoShape) juce::jlimit (0, 4, iv (ids::lfo1Shape));
    s.lfo2Rate = v (ids::lfo2Rate); s.lfo2Shape = (zen::LfoShape) juce::jlimit (0, 4, iv (ids::lfo2Shape));
    s.randRate = v (ids::randRate); s.randSmooth = v (ids::randSmooth);
    for (int i = 0; i < zen::numSlots; ++i)
    {
        auto& sl = s.slots[(size_t) i];
        sl.src = (zen::Src) juce::jlimit (0, zen::numSrc - 1, (int) std::lround (slotSrcP[(size_t) i]->load()));
        sl.dst = (zen::Dst) juce::jlimit (0, zen::numDst - 1, (int) std::lround (slotDstP[(size_t) i]->load()));
        sl.amount = slotAmtP[(size_t) i]->load();
    }
    s.glide = v (ids::glide); s.spread = v (ids::spread); s.volume = v (ids::volume); s.velSens = v (ids::velSens);
    s.chorusMix = v (ids::chorusMix); s.chorusDepth = v (ids::chorusDepth); s.chorusRate = v (ids::chorusRate);
    s.delayTime = v (ids::delayTime); s.delaySync = iv (ids::delaySync); s.delayFeedback = v (ids::delayFeedback);
    s.delayTone = v (ids::delayTone); s.delayMix = v (ids::delayMix);
    s.reverbSize = v (ids::reverbSize); s.reverbDecay = v (ids::reverbDecay); s.reverbDamp = v (ids::reverbDamp);
    s.reverbShimmer = v (ids::reverbShimmer); s.reverbMix = v (ids::reverbMix);
    s.root = iv (ids::root); s.scale = iv (ids::scale); s.octave = iv (ids::octave) - 3;
    s.latchTimbre = bv (ids::latchTimbre); s.padChannel = iv (ids::padChannel); s.platesFromPads = bv (ids::platesFromPads);
    for (int p = 0; p < zen::numPlates; ++p) s.padNotes[(size_t) p] = padNotes[(size_t) p].load();
    s.arpOn = bv (ids::arpOn); s.arpMode = (zen::ArpMode) juce::jlimit (0, 4, iv (ids::arpMode));
    s.arpRate = iv (ids::arpRate); s.arpOctaves = iv (ids::arpOctaves) + 1; s.arpGate = v (ids::arpGate); s.arpHold = bv (ids::arpHold);
    {
        static const float ranges[] = { 1, 2, 3, 4, 5, 7, 12, 24 };
        s.bendRange = ranges[juce::jlimit (0, 7, iv (ids::bendRange))];
        static const int mpeRanges[] = { 0, 12, 24, 48, 96 };
        s.mpe = mpeRanges[juce::jlimit (0, 4, iv (ids::mpe))];
    }
    s.loopLevel = loopLevel.load();
    s.bpm = hostBpm.load();
    s.hostPpq = hostPpq.load();

    // the drum pattern: copy it only when the UI changed it (try_lock: never wait on the audio thread)
    auto* self = const_cast<ZenProcessor*> (this);
    if (drumVersion.load() != drumAudioVersion)
    {
        std::unique_lock<std::mutex> l (drumLock, std::try_to_lock);
        if (l.owns_lock()) { self->drumsForAudio = drumSettings; self->drumAudioVersion = drumVersion.load(); }
    }
    s.drums = drumsForAudio;
    return s;
}

void ZenProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    double ppqNow = -1.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto bpm = pos->getBpm()) hostBpm.store ((float) *bpm);
            if (pos->getIsPlaying())
                if (auto ppq = pos->getPpqPosition()) ppqNow = *ppq;
        }
    hostPpq.store (ppqNow);

    events.clear();
    for (const auto meta : midi)
    {
        const auto* d = meta.data;
        if (meta.numBytes < 1) continue;
        zen::MidiEvent e;
        e.offset = meta.samplePosition;
        e.status = d[0];
        e.d1 = meta.numBytes > 1 ? d[1] : 0;
        e.d2 = meta.numBytes > 2 ? d[2] : 0;
        if ((e.status & 0xF0) >= 0x80 && (e.status & 0xF0) <= 0xE0) events.push_back (e);
    }

    const auto s = readSettings();
    const int n = buffer.getNumSamples();
    buffer.clear();
    if (buffer.getNumChannels() < 2) return;
    engine.process (s, events.data(), (int) events.size(), buffer.getWritePointer (0), buffer.getWritePointer (1), n);
}

//==============================================================================
void ZenProcessor::setPadNote (int plate, int note)
{
    if (! juce::isPositiveAndBelow (plate, zen::numPlates)) return;
    // a note can only drive one plate
    for (int p = 0; p < zen::numPlates; ++p) if (p != plate && padNotes[(size_t) p].load() == note) padNotes[(size_t) p].store (-1);
    padNotes[(size_t) plate].store (note);
}

void ZenProcessor::resetPadNotes()
{
    const auto d = zen::defaultPadNotes();
    for (int p = 0; p < zen::numPlates; ++p) padNotes[(size_t) p].store (d[(size_t) p]);
}

int ZenProcessor::findSlot (zen::Src src, zen::Dst dst) const
{
    for (int i = 0; i < zen::numSlots; ++i)
        if ((int) std::lround (slotSrcP[(size_t) i]->load()) == (int) src && (int) std::lround (slotDstP[(size_t) i]->load()) == (int) dst)
            return i;
    return -1;
}

int ZenProcessor::findOrMakeSlot (zen::Src src, zen::Dst dst)
{
    if (const int i = findSlot (src, dst); i >= 0) return i;
    for (int i = 0; i < zen::numSlots; ++i)
        if ((int) std::lround (slotSrcP[(size_t) i]->load()) == 0 || (int) std::lround (slotDstP[(size_t) i]->load()) == 0)
        {
            auto set = [this] (const juce::String& id, float value)
            {
                if (auto* p = apvts.getParameter (id))
                {
                    p->beginChangeGesture(); p->setValueNotifyingHost (p->convertTo0to1 (value)); p->endChangeGesture();
                }
            };
            set (ids::slotSrc (i), (float) (int) src);
            set (ids::slotDst (i), (float) (int) dst);
            set (ids::slotAmt (i), 0.0f);
            return i;
        }
    return -1;
}

//==============================================================================
std::vector<ZenProcessor::PresetInfo> ZenProcessor::presetInfo()
{
    std::vector<PresetInfo> out;
    for (auto& p : presets::all()) out.push_back ({ p.name, p.category });
    return out;
}

int ZenProcessor::getNumPrograms() { return (int) presets::all().size(); }

const juce::String ZenProcessor::getProgramName (int index)
{
    return juce::isPositiveAndBelow (index, getNumPrograms()) ? presets::all()[(size_t) index].name : "";
}

void ZenProcessor::setCurrentProgram (int index)
{
    if (! juce::isPositiveAndBelow (index, getNumPrograms())) return;
    currentPreset = index;
    userPresetName.clear();

    auto setParam = [this] (const juce::String& id, float value)
    {
        if (auto* rp = apvts.getParameter (id))
        {
            rp->beginChangeGesture();
            rp->setValueNotifyingHost (rp->convertTo0to1 (value));
            rp->endChangeGesture();
        }
    };

    // Defaults first. Play settings (scale, root, octave, pads) are the player's, so they stay.
    static const juce::StringArray keep { ids::root, ids::scale, ids::octave, ids::padChannel, ids::platesFromPads, ids::latchTimbre, ids::bendRange, ids::mpe };
    for (auto* param : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (param))
        {
            if (keep.contains (rp->getParameterID())) continue;
            rp->beginChangeGesture();
            rp->setValueNotifyingHost (rp->getDefaultValue());
            rp->endChangeGesture();
        }

    const auto& pr = presets::all()[(size_t) index];
    for (auto& val : pr.values) setParam (val.id, val.v);

    // Mod slots: the plate kit first, then the preset's own.
    if (index > 0)
    {
        auto slots = presets::kit();
        for (auto& sl : pr.slots) slots.push_back (sl);
        for (int i = 0; i < zen::numSlots && i < (int) slots.size(); ++i)
        {
            setParam (ids::slotSrc (i), (float) (int) slots[(size_t) i].s);
            setParam (ids::slotDst (i), (float) (int) slots[(size_t) i].d);
            setParam (ids::slotAmt (i), slots[(size_t) i].amt);
        }
    }
}

//==============================================================================
void ZenProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("preset", currentPreset, nullptr);
    state.setProperty ("visuals", visualsMode.load(), nullptr);
    juce::String pads;
    for (int p = 0; p < zen::numPlates; ++p) pads << padNotes[(size_t) p].load() << (p < zen::numPlates - 1 ? "," : "");
    state.setProperty ("padNotes", pads, nullptr);
    state.setProperty ("userPreset", userPresetName, nullptr);
    state.setProperty ("loopLevel", loopLevel.load(), nullptr);
    {
        const auto d = getDrums();
        auto join = [] (const std::array<float, zen::numDrums>& a)
        {
            juce::StringArray sa; for (float v : a) sa.add (juce::String (v, 4)); return sa.joinIntoString (",");
        };
        state.setProperty ("drumPattern", juce::String (zen::patternToString (d)), nullptr);
        state.setProperty ("drumTempo", d.tempo, nullptr);
        state.setProperty ("drumSwing", d.swing, nullptr);
        state.setProperty ("drumVolume", d.volume, nullptr);
        state.setProperty ("drumLevel", join (d.level), nullptr);
        state.setProperty ("drumTune", join (d.tune), nullptr);
        state.setProperty ("drumDecay", join (d.decay), nullptr);
        juce::StringArray snd; for (int v : d.sound) snd.add (juce::String (v));
        state.setProperty ("drumSounds", snd.joinIntoString (","), nullptr);
        for (int r = 0; r < zen::numDrums; ++r)
        {
            const auto& pr = patchRecipes[(size_t) r];
            if (pr.kind == 0 || snapshotting) continue;
            const juce::String k = "drumPatch" + juce::String (r);
            state.setProperty (k + "Kind", pr.kind, nullptr);
            state.setProperty (k + "Name", pr.name, nullptr);
            if (pr.kind == 2) state.setProperty (k + "File", pr.file, nullptr);
            if (pr.kind == 3) state.setProperty (k + "State", pr.state.toBase64Encoding(), nullptr);
        }
    }

    const auto k = getKeyframes();
    juce::MemoryBlock mb;
    for (auto& c : k) mb.append (c.data(), c.size() * sizeof (float));
    state.setProperty ("ownFrames", (int) k.size(), nullptr);
    state.setProperty ("ownWaves", mb.toBase64Encoding(), nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void ZenProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            currentPreset = tree.getProperty ("preset", 1);
            visualsMode.store ((int) tree.getProperty ("visuals", 2));
            auto pads = juce::StringArray::fromTokens (tree.getProperty ("padNotes", "").toString(), ",", "");
            if (pads.size() == zen::numPlates)
                for (int p = 0; p < zen::numPlates; ++p) padNotes[(size_t) p].store (pads[p].getIntValue());
            userPresetName = tree.getProperty ("userPreset", "").toString();
            loopLevel.store ((float) (double) tree.getProperty ("loopLevel", 0.8));
            if (tree.hasProperty ("drumPattern"))
            {
                auto d = getDrums();
                zen::patternFromString (d, tree.getProperty ("drumPattern").toString().toStdString());
                d.tempo  = (float) (double) tree.getProperty ("drumTempo", 90.0);
                d.swing  = (float) (double) tree.getProperty ("drumSwing", 0.0);
                d.volume = (float) (double) tree.getProperty ("drumVolume", 0.6);
                auto split = [&tree] (const char* id, std::array<float, zen::numDrums>& a)
                {
                    auto sa = juce::StringArray::fromTokens (tree.getProperty (id).toString(), ",", "");
                    if (sa.size() == zen::numDrums) for (int i = 0; i < zen::numDrums; ++i) a[(size_t) i] = sa[i].getFloatValue();
                };
                split ("drumLevel", d.level); split ("drumTune", d.tune); split ("drumDecay", d.decay);
                if (auto sa = juce::StringArray::fromTokens (tree.getProperty ("drumSounds").toString(), ",", ""); sa.size() == zen::numDrums)
                    for (int i = 0; i < zen::numDrums; ++i) d.sound[(size_t) i] = sa[i].getIntValue();
                d.playing = false;   // a reopened session starts quiet
                setDrums (d);

                // captured patches: keep the recipes, capture them again (on the message thread)
                std::vector<int> rows;
                for (int r = 0; r < zen::numDrums; ++r)
                {
                    const juce::String k = "drumPatch" + juce::String (r);
                    PatchRecipe pr;
                    pr.kind = (int) tree.getProperty (k + "Kind", 0);
                    pr.name = tree.getProperty (k + "Name", "").toString();
                    pr.file = tree.getProperty (k + "File", "").toString();
                    if (pr.kind == 3) pr.state.fromBase64Encoding (tree.getProperty (k + "State", "").toString());
                    patchRecipes[(size_t) r] = pr;
                    if (pr.kind != 0 && d.sound[(size_t) r] == (int) zen::Sound::patch) rows.push_back (r);
                }
                if (! rows.empty() && ! lightweight)
                    juce::MessageManager::callAsync ([alive = std::weak_ptr<int> (lifeToken), this, rows]
                    {
                        if (alive.lock() == nullptr) return;
                        for (int r : rows) recaptureDrumPatch (r);
                    });
            }
            applyState (tree, true);
        }
}

juce::AudioProcessorEditor* ZenProcessor::createEditor() { return new ZenEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ZenProcessor(); }
