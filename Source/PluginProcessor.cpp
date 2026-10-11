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
             "Plate A", "Plate B", "Plate C", "Plate D", "Key" };
}
juce::StringArray ZenProcessor::srcShortNames()
{
    return { "-", "ENV1", "ENV2", "LFO1", "LFO2", "RAND", "PRESS", "VEL", "WHEEL", "AT", "A", "B", "C", "D", "KEY" };
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

    return layout;
}

//==============================================================================
ZenProcessor::ZenProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "ZenthesizerState", createLayout())
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
    setCurrentProgram (1);   // open on the first real preset
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
    engine.prepare (sampleRate);
    events.reserve ((size_t) std::max (256, samplesPerBlock));
}

zen::Settings ZenProcessor::readSettings() const
{
    zen::Settings s;
    s.aWave = (zen::WaveA) juce::jlimit (0, 3, iv (ids::aWave));
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
    s.bpm = hostBpm.load();
    return s;
}

void ZenProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto bpm = pos->getBpm())
                hostBpm.store ((float) *bpm);

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
    static const juce::StringArray keep { ids::root, ids::scale, ids::octave, ids::padChannel, ids::platesFromPads, ids::latchTimbre };
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
            apvts.replaceState (tree);
        }
}

juce::AudioProcessorEditor* ZenProcessor::createEditor() { return new ZenEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ZenProcessor(); }
