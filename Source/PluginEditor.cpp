#include "PluginEditor.h"
#include <map>
#include "dsp/Maps.h"

#if JUCE_ANDROID || JUCE_IOS
 #include <juce_audio_devices/juce_audio_devices.h>
 #include <juce_audio_utils/juce_audio_utils.h>
 #include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>
#endif

namespace
{
    constexpr float C = 490.0f;
    const juce::Point<float> centre (C, C);

    juce::Point<float> pol (float r, float deg) { return theme::polar (centre, r, deg); }

}

//==============================================================================
EngineBox::EngineBox (ZenProcessor& p, bool b) : proc (p), isB (b)
{
    setPaintingIsUnclipped (true);
    const char* choiceId = isB ? ids::bTable : ids::aWave;
    const juce::StringArray names = isB ? juce::StringArray { "AIR", "GLASS", "VOX", "REED", "OWN" } : juce::StringArray { "SAW", "SQR", "TRI", "SIN", "STR" };
    for (int i = 0; i < names.size(); ++i)
    {
        selectors.push_back (std::make_unique<SteelButton> (names[i], 18.0f));
        auto* s = selectors[(size_t) i].get();
        addAndMakeVisible (s);
        s->onClick = [this, choiceId, i]
        {
            if (auto* prm = proc.apvts.getParameter (choiceId))
            {
                prm->beginChangeGesture(); prm->setValueNotifyingHost (prm->convertTo0to1 ((float) i)); prm->endChangeGesture();
            }
        };
    }

    struct K { const char* label; const char* id; zen::Dst dst; };
    const K a[4] = { { "TUNE", ids::aTune, zen::Dst::aTune }, { "DETUNE", ids::aDetune, zen::Dst::aDetune },
                     { "SHAPE", ids::aShape, zen::Dst::aShape }, { "LEVEL", ids::aLevel, zen::Dst::aLevel } };
    const K bk[4] = { { "POS", ids::bPos, zen::Dst::bPos }, { "WARP", ids::bWarp, zen::Dst::bWarp },
                      { "UNISON", ids::bUnison, zen::Dst::bUnison }, { "LEVEL", ids::bLevel, zen::Dst::bLevel } };
    for (int i = 0; i < 4; ++i)
    {
        const auto& k = isB ? bk[i] : a[i];
        knobs[(size_t) i] = std::make_unique<WoodKnob> (k.label, 32.0f);
        knobs[(size_t) i]->dst = (int) k.dst;
        addAndMakeVisible (*knobs[(size_t) i]);
        attachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, k.id, *knobs[(size_t) i]));
    }
    if (! isB) knobs[0]->setDoubleClickReturnValue (true, 0.5);
}

void EngineBox::resized()
{
    screen = { 14.0f, 34.0f, (float) getWidth() - 28.0f, 40.0f };
    const float step = ((float) getWidth() - 20.0f) / 4.0f;
    const float selStep = ((float) getWidth() - 16.0f) / (float) selectors.size();
    for (size_t i = 0; i < selectors.size(); ++i)
        selectors[i]->setBounds ((int) (8.0f + selStep * (float) i), 80, (int) selStep, 46);
    for (int i = 0; i < 4; ++i)
        knobs[(size_t) i]->setBounds ((int) (10.0f + step * (float) i) - 4, 128, (int) step + 8, 64);
}

void EngineBox::tick (float dt, bool animate)
{
    if (animate) phase += dt * 0.35f;
    const int sel = (int) std::lround (proc.apvts.getRawParameterValue (isB ? ids::bTable : ids::aWave)->load());
    for (int i = 0; i < (int) selectors.size(); ++i)
        if (selectors[(size_t) i]->getToggleState() != (i == sel)) selectors[(size_t) i]->setToggleState (i == sel, juce::dontSendNotification);
    repaint (screen.toNearestInt().expanded (6));
}

void EngineBox::mouseDown (const juce::MouseEvent& e)
{
    if (isB && screen.expanded (4.0f).contains (e.position) && onScreenClick) onScreenClick();
}

void EngineBox::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat().reduced (1.0f);
    juce::Path outline;
    outline.addRoundedRectangle (b.getX(), b.getY(), b.getWidth(), b.getHeight(), 32.0f, 26.0f,
                                 ! isB, isB, isB, ! isB);
    g.setColour (theme::burn.withAlpha (0.16f));
    g.strokePath (outline, juce::PathStrokeType (3.5f));
    g.setColour (theme::burn.withAlpha (0.55f));
    g.strokePath (outline, juce::PathStrokeType (1.4f));

    theme::burnText (g, isB ? "B" : "A", { 18.0f, 12.0f, 40.0f, 14.0f }, juce::Justification::centredLeft,
                     theme::font (theme::FontKind::sansBold, 12.0f, 0.2f));
    theme::burnText (g, isB ? "WAVETABLE" : "ANALOG", { b.getRight() - 120.0f, 12.0f, 104.0f, 14.0f }, juce::Justification::centredRight,
                     theme::font (theme::FontKind::sans, 11.0f, 0.2f));

    // carved glass screen
    g.setColour (juce::Colour (0x59ffecc8));
    g.fillRoundedRectangle (screen.expanded (4.0f).translated (0, 1.0f), 16.0f);
    g.setColour (juce::Colour (0x8c3b1f0c));
    g.fillRoundedRectangle (screen.expanded (3.0f), 16.0f);
    g.setColour (theme::glass);
    g.fillRoundedRectangle (screen, 14.0f);
    {
        juce::Graphics::ScopedSaveState ss (g);
        juce::Path clip; clip.addRoundedRectangle (screen, 14.0f);
        g.reduceClipRegion (clip);
        if (isB) paintScreenB (g, screen); else paintScreenA (g, screen);
        juce::ColourGradient sh (juce::Colours::black.withAlpha (0.7f), screen.getX(), screen.getY(), juce::Colours::transparentBlack, screen.getX(), screen.getY() + 10.0f, false);
        g.setGradientFill (sh);
        g.fillRect (screen);
    }
}

void EngineBox::paintScreenA (juce::Graphics& g, juce::Rectangle<float> r)
{
    const int wave = (int) std::lround (proc.apvts.getRawParameterValue (ids::aWave)->load());
    float shape = proc.apvts.getRawParameterValue (ids::aShape)->load();
    const float live = proc.display().dstNorm[(int) zen::Dst::aShape].load();
    if (live >= 0.0f) shape = live;

    zen::AnalogOsc osc;
    osc.phase = std::fmod (phase, 1.0f);
    juce::Path p;
    const int n = 160;
    float vals[n];
    if (wave == (int) zen::WaveA::string)
    {
        // a plucked string ringing down: brighter with SHAPE, a fizzing edge at the top of it
        const float tone = std::min (1.0f, shape / 0.66f), buzz = juce::jlimit (0.0f, 1.0f, (shape - 0.66f) / 0.34f);
        const float pluckAge = std::fmod (phase * 0.6f, 1.0f);
        float pk = 1.0e-6f;
        for (int i = 0; i < n; ++i)
        {
            const float x = (float) i / (float) n;
            const float t = x * 3.0f + phase;
            float y = 0.0f;
            for (int h = 1; h <= 9; ++h)
                y += std::pow (0.25f + 0.6f * tone, (float) (h - 1)) / (float) h * std::sin (juce::MathConstants<float>::twoPi * (float) h * t);
            if (buzz > 0.0f && y > 0.35f) y += buzz * 0.5f * std::sin (juce::MathConstants<float>::twoPi * 23.0f * t);
            vals[i] = y * std::exp (-2.2f * (x + pluckAge));
            pk = std::max (pk, std::abs (vals[i]));
        }
        for (auto& v : vals) v /= pk * 1.1f;
    }
    else
    {
        for (int i = 0; i < 40; ++i) osc.process ((zen::WaveA) wave, shape, 2.0f / (float) n);   // settle the feedback
        osc.phase = std::fmod (phase, 1.0f);
        for (int i = 0; i < n; ++i) vals[i] = osc.process ((zen::WaveA) wave, shape, 2.5f / (float) n);
    }
    for (int i = 0; i < n; ++i)
    {
        const float x = r.getX() + r.getWidth() * (float) i / (float) (n - 1);
        const float y = r.getCentreY() - vals[i] * r.getHeight() * 0.36f;
        if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
    }
    g.setColour (theme::glow.withAlpha (0.22f));
    g.strokePath (p, juce::PathStrokeType (6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (theme::glow);
    g.strokePath (p, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void EngineBox::paintScreenB (juce::Graphics& g, juce::Rectangle<float> r)
{
    const int table = (int) std::lround (proc.apvts.getRawParameterValue (ids::bTable)->load());
    std::shared_ptr<const zen::UserTable> own;
    zen::TableView view;
    if (table == zen::userTable && (own = proc.getUserTable()) != nullptr) view = own->view();
    else view = zen::Wavetables::get().view (std::min (zen::numTables - 1, table));

    float pos = proc.apvts.getRawParameterValue (ids::bPos)->load();
    const float live = proc.display().dstNorm[(int) zen::Dst::bPos].load();
    if (live >= 0.0f) pos = live;

    auto frameAt = [&] (float fp, float yOff, float xOff, float amp, juce::Colour col, float width, bool glowLine)
    {
        juce::Path p;
        const int n = 90;
        for (int i = 0; i < n; ++i)
        {
            const float ph = (float) i / (float) (n - 1);
            const float v = zen::WaveReader::read (view, fp, ph, 1.0f / 256.0f);
            const float x = r.getX() + 8.0f + xOff + (r.getWidth() - 30.0f) * ph;
            const float y = r.getCentreY() + yOff - v * amp;
            if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        if (glowLine) { g.setColour (col.withAlpha (0.22f)); g.strokePath (p, juce::PathStrokeType (6.0f)); }
        g.setColour (col);
        g.strokePath (p, juce::PathStrokeType (width, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    };
    const int frames = std::max (1, view.frames);
    for (int f = 0; f < frames && frames > 1; ++f)
    {
        const float fp = (float) f / (float) (frames - 1);
        const float off = (fp - 0.5f) * 2.0f;
        frameAt (fp, off * 9.0f, (1.0f - fp) * 12.0f, r.getHeight() * 0.22f, theme::glow.withAlpha (0.12f + 0.1f * (1.0f - std::abs (fp - pos) * 2.0f)), 1.0f, false);
    }
    frameAt (pos, frames > 1 ? (pos - 0.5f) * 18.0f : 0.0f, frames > 1 ? (1.0f - pos) * 12.0f : 6.0f, r.getHeight() * 0.3f, theme::glow, 2.0f, true);

    if (table == zen::userTable)
    {
        g.setColour (theme::glow.withAlpha (0.55f));
        g.setFont (theme::font (theme::FontKind::sans, 8.5f, 0.12f));
        g.drawText ("TAP TO SCULPT", r.reduced (8.0f, 3.0f), juce::Justification::bottomRight, false);
    }
}

//==============================================================================
SlotRow::SlotRow (ZenProcessor& p, int i)
    : proc (p), index (i),
      srcAttach (p.apvts, ids::slotSrc (i), src),
      dstAttach (p.apvts, ids::slotDst (i), dst),
      amtAttach (p.apvts, ids::slotAmt (i), amount)
{
    src.addItemList (ZenProcessor::srcNames(), 1);
    dst.addItemList (ZenProcessor::dstNames(), 1);
    // re-sync after the items exist
    src.setSelectedItemIndex ((int) std::lround (p.apvts.getRawParameterValue (ids::slotSrc (i))->load()), juce::dontSendNotification);
    dst.setSelectedItemIndex ((int) std::lround (p.apvts.getRawParameterValue (ids::slotDst (i))->load()), juce::dontSendNotification);
    addAndMakeVisible (src); addAndMakeVisible (dst); addAndMakeVisible (amount); addAndMakeVisible (clear);
    amount.textFromValueFunction = [] (double v) { return juce::String (juce::roundToInt (v * 100.0)) + "%"; };
    clear.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    clear.setColour (juce::TextButton::textColourOffId, theme::burn);
    clear.setTooltip ("Clear this slot");
    clear.onClick = [this]
    {
        for (auto id : { ids::slotSrc (index), ids::slotDst (index), ids::slotAmt (index) })
            if (auto* prm = proc.apvts.getParameter (id))
            {
                prm->beginChangeGesture(); prm->setValueNotifyingHost (prm->getDefaultValue()); prm->endChangeGesture();
            }
    };
    src.onChange = [this] { refreshColour(); };
    refreshColour();
}

void SlotRow::refreshColour()
{
    amount.colour = theme::srcColour (src.getSelectedItemIndex());
    repaint();
}

void SlotRow::resized()
{
    src.setBounds (14, 2, 92, 26);
    dst.setBounds (124, 2, 104, 26);
    amount.setBounds (230, 2, 92, 30);
    clear.setBounds (322, 4, 22, 22);
}

void SlotRow::paint (juce::Graphics& g)
{
    const auto col = theme::srcColour (src.getSelectedItemIndex());
    g.setColour (col.withAlpha (0.3f)); g.fillEllipse (2.0f, 11.0f, 8.0f, 8.0f);
    g.setColour (col.interpolatedWith (theme::ledOff, src.getSelectedItemIndex() == 0 ? 1.0f : 0.0f));
    g.fillEllipse (3.5f, 12.5f, 5.0f, 5.0f);
    juce::Path arrow;
    arrow.startNewSubPath (108.0f, 15.0f); arrow.lineTo (120.0f, 15.0f);
    arrow.addTriangle (120.0f, 11.5f, 120.0f, 18.5f, 124.0f, 15.0f);
    g.setColour (theme::burn.withAlpha (0.7f));
    g.strokePath (arrow, juce::PathStrokeType (1.4f));
    g.fillPath (arrow);
}

//==============================================================================
Content::Content (ZenProcessor& p)
    : proc (p), porthole (p.display()), engineA (p, false), engineB (p, true), wavePanel (p), beatPanel (p)
{
    setLookAndFeel (&lnf);
    setOpaque (true);

    // ---- tabs, visuals switch, preset arrows --------------------------------------
    const char* tabNames[] = { "SYNTH", "MOD", "WAVES", "BEAT", "FX", "PLAY", "STAGE" };
    for (int i = 0; i < numPages; ++i)
    {
        tabs[(size_t) i] = std::make_unique<TabButton> (tabNames[i]);
        addAndMakeVisible (*tabs[(size_t) i]);
        const float a = -37.5f + 12.5f * (float) i;
        place (*tabs[(size_t) i], pol (334.0f, a), 70.0f, 34.0f, a);
        tabs[(size_t) i]->onClick = [this, i] { showPage (i); };
    }
    const char* visNames[] = { "OFF", "LOW", "FULL" };
    for (int i = 0; i < 3; ++i)
    {
        visButtons[(size_t) i] = std::make_unique<SteelButton> (visNames[i], 20.0f);
        addAndMakeVisible (*visButtons[(size_t) i]);
        const float a = 51.0f + 9.0f * (float) i;
        place (*visButtons[(size_t) i], pol (352.0f, a), 48.0f, 54.0f, a, 22.0f);
        visButtons[(size_t) i]->onClick = [this, i] { proc.visualsMode.store (i); };
    }
    for (auto* b : { &prevBtn, &nextBtn })
    {
        addAndMakeVisible (b);
        b->showLed = false;
    }
    place (prevBtn, pol (382.0f, 204.0f), 48.0f, 40.0f, 24.0f, 8.0f);
    place (nextBtn, pol (382.0f, 156.0f), 48.0f, 40.0f, -24.0f, 8.0f);
    prevBtn.onClick = [this] { stepPreset (-1); };
    nextBtn.onClick = [this] { stepPreset (1); };
    // ---- the looper, along the bottom right of the slab (always there, on every page)
    {
        const float angles[3] = { 121.0f, 133.0f, 145.0f };
        SteelButton* btns[3] = { &loopBtn, &loopStopBtn, &loopUndoBtn };
        for (int i = 0; i < 3; ++i)
        {
            addAndMakeVisible (btns[i]);
            place (*btns[i], pol (388.0f, angles[i]), 54.0f, 56.0f, 0.0f, 23.0f);
        }
        loopBtn.setTriggeredOnMouseDown (true);       // timing matters: act on the touch, not the release
        loopStopBtn.setTriggeredOnMouseDown (true);
        loopStopBtn.showLed = false;
        loopBtn.onClick = [this] { proc.looper().requestLoop(); };
        loopStopBtn.onClick = [this] { proc.looper().requestStop(); };
        loopUndoBtn.onHold = [this] { proc.looper().requestClear(); showHint ("loop cleared"); };
        loopUndoBtn.onClick = [this]
        {
            if (loopUndoBtn.consumeHold()) return;
            if (proc.looper().displayCanUndo.load()) { proc.looper().requestUndo(); showHint ("last overdub undone"); }
            else showHint ("nothing to undo  -  hold UNDO to clear the loop");
        };
        loopBtn.setTooltip ("LOOP: record, then play, then overdub. Records what you hear, so you can change presets and play over it.");
        loopUndoBtn.setTooltip ("Tap: undo the last overdub. Hold: clear the loop.");

        addAndMakeVisible (loopLevelKnob);
        place (loopLevelKnob, pol (386.0f, 237.0f), 70.0f, 66.0f, 0.0f, 23.0f);
        loopLevelKnob.setRange (0.0, 1.0);
        loopLevelKnob.setValue (proc.loopLevel.load(), juce::dontSendNotification);
        loopLevelKnob.setDoubleClickReturnValue (true, 0.8);
        loopLevelKnob.textFromValueFunction = [] (double v) { return juce::String (juce::roundToInt (v * 100.0)) + "%"; };
        loopLevelKnob.onValueChange = [this] { proc.loopLevel.store ((float) loopLevelKnob.getValue()); };
    }

    // ---- PITCH and MOD strips along the slab's edges (on every page)
    addAndMakeVisible (pitchStrip);
    addAndMakeVisible (modStrip);
    pitchStrip.onChange = [this] (float v) { proc.setUiBend (v); };
    modStrip.onChange   = [this] (float v) { proc.setUiWheel (v); };
    pitchStrip.setTooltip ("Pitch bend: slide up or down, it springs back when you let go. Range is on the PLAY page.");
    modStrip.setTooltip ("Mod wheel: slide to set, it stays. Your keyboard's mod wheel moves it too.");

    addAndMakeVisible (presetHit);
    presetHit.setBounds (390, 838, 200, 40);
    presetHit.addMouseListener (this, false);
    presetHit.setMouseCursor (juce::MouseCursor::PointingHandCursor);

    // ---- plates --------------------------------------------------------------------
    for (int i = 0; i < zen::numPlates; ++i)
    {
        const bool timbre = i >= zen::numNotePlates;
        const float dia = timbre ? 56.0f : 46.0f;
        plates[(size_t) i] = std::make_unique<BrassPlate> (timbre ? juce::String::charToString ((juce::juce_wchar) ('A' + i - 12)) : juce::String (i + 1), dia);
        auto* pl = plates[(size_t) i].get();
        addAndMakeVisible (pl);
        static const float timbreAngles[4] = { 213.0f, 193.0f, 167.0f, 147.0f };
        const juce::Point<float> pos = timbre ? pol (200.0f, timbreAngles[i - 12])
                                              : pol (318.0f, 244.0f - (float) i * 128.0f / 11.0f);
        place (*pl, pos, dia + 70.0f, dia + 70.0f);
        pl->onPress = [this, i] (float pr)
        {
            if (learning) { learnPlate = i; lastInCount = proc.display().inCount.load(); return; }
            proc.setUiPlate (i, pr);
        };
        pl->onRelease = [this, i] { if (! learning) proc.setUiPlate (i, -1.0f); };
        if (timbre) pl->onRightClick = [this, i] { arm ((int) zen::Src::plateA + (i - 12)); };
        pl->setTooltip (timbre ? "Timbre plate: hold to play its modulation. Double-tap to lock it on (double-tap again to unlock). Right-click to assign it."
                               : "Note plate: click and hold to play, drag up for more pressure.");
    }

    // ---- SYNTH page ------------------------------------------------------------------
    setupSynthPage();

    // ---- MOD page -----------------------------------------------------------------------
    // Everything on the inner pages stays inside a circle of ~300 around the centre,
    // clear of the tabs, the visuals switch and the plates.
    {
        struct K { const char* label; const char* id; float x, y; };
        const K ks[] = {
            { "ATTACK", ids::e1A, 283, 290 }, { "DECAY", ids::e1D, 339, 290 }, { "SUSTAIN", ids::e1S, 395, 290 }, { "RELEASE", ids::e1R, 451, 290 },
            { "ATTACK", ids::e2A, 529, 290 }, { "DECAY", ids::e2D, 585, 290 }, { "SUSTAIN", ids::e2S, 641, 290 }, { "RELEASE", ids::e2R, 697, 290 },
            { "RATE", ids::lfo1Rate, 236, 388 }, { "RATE", ids::lfo2Rate, 392, 388 },
            { "RATE", ids::randRate, 546, 388 }, { "SMOOTH", ids::randSmooth, 600, 388 },
            { "KEY", ids::keyTrack, 666, 388 }, { "VELOCITY", ids::velSens, 724, 388 },
        };
        for (auto& k : ks)
        {
            auto kn = std::make_unique<WoodKnob> (k.label, 34.0f);
            addAndMakeVisible (*kn);
            place (*kn, { k.x, k.y }, 60.0f, 62.0f, 0.0f, 6.0f + 17.0f);
            sliderAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, k.id, *kn));
            pageComps[mod].push_back (kn.get());
            modPageKnobs.push_back (std::move (kn));
        }
        for (auto* cb : { &lfo1Shape, &lfo2Shape }) { cb->addItemList (ZenProcessor::lfoShapeNames(), 1); addAndMakeVisible (cb); pageComps[mod].push_back (cb); }
        lfo1Shape.setBounds (268, 376, 82, 24);
        lfo2Shape.setBounds (424, 376, 82, 24);
        comboAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, ids::lfo1Shape, lfo1Shape));
        comboAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, ids::lfo2Shape, lfo2Shape));

        for (int i = 0; i < zen::numSlots; ++i)
        {
            slotRows[(size_t) i] = std::make_unique<SlotRow> (proc, i);
            addChildComponent (*slotRows[(size_t) i]);
            slotRows[(size_t) i]->setBounds (306, 448 + (i % 6) * 29, 350, 31);
        }
        for (int i = 0; i < 2; ++i)
        {
            slotPageBtns[(size_t) i] = std::make_unique<TabButton> (i == 0 ? "1-6" : "7-12");
            addAndMakeVisible (*slotPageBtns[(size_t) i]);
            slotPageBtns[(size_t) i]->setBounds (628 + i * 56, 418, 52, 28);
            slotPageBtns[(size_t) i]->onClick = [this, i] { slotPage = i; showPage (mod); };
            pageComps[mod].push_back (slotPageBtns[(size_t) i].get());
        }
        pageLabels.push_back ({ mod, "ENV 1", { 367, 250 } });
        pageLabels.push_back ({ mod, "ENV 2", { 613, 250 } });
        pageLabels.push_back ({ mod, "LFO 1", { 292, 346 } });
        pageLabels.push_back ({ mod, "LFO 2", { 448, 346 } });
        pageLabels.push_back ({ mod, "RANDOM", { 573, 346 } });
        pageLabels.push_back ({ mod, "VOICE", { 695, 346 } });
        pageLabels.push_back ({ mod, "MODULATION", { 470, 432 } });
    }

    // ---- FX page -------------------------------------------------------------------------
    {
        struct K { const char* label; const char* id; float x, y; };
        const K ks[] = {
            { "MIX", ids::chorusMix, 300, 295 }, { "DEPTH", ids::chorusDepth, 356, 295 }, { "RATE", ids::chorusRate, 412, 295 },
            { "AIR", ids::air, 520, 295 }, { "GLIDE", ids::glide, 576, 295 }, { "SPREAD", ids::spread, 632, 295 }, { "VOLUME", ids::volume, 688, 295 },
            { "TIME", ids::delayTime, 326, 405 }, { "FEEDBACK", ids::delayFeedback, 506, 405 }, { "TONE", ids::delayTone, 570, 405 }, { "MIX", ids::delayMix, 634, 405 },
            { "SIZE", ids::reverbSize, 360, 515 }, { "DECAY", ids::reverbDecay, 425, 515 }, { "DAMPING", ids::reverbDamp, 490, 515 },
            { "SHIMMER", ids::reverbShimmer, 555, 515 }, { "MIX", ids::reverbMix, 620, 515 },
        };
        for (auto& k : ks)
        {
            auto kn = std::make_unique<WoodKnob> (k.label, 36.0f);
            addAndMakeVisible (*kn);
            place (*kn, { k.x, k.y }, 62.0f, 64.0f, 0.0f, 6.0f + 18.0f);
            sliderAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, k.id, *kn));
            pageComps[fx].push_back (kn.get());
            if (std::string (k.id) == ids::delayMix) kn->dst = (int) zen::Dst::delayMix;
            if (std::string (k.id) == ids::reverbMix) kn->dst = (int) zen::Dst::reverbMix;
            if (std::string (k.id) == ids::chorusMix) kn->dst = (int) zen::Dst::chorusMix;
            if (std::string (k.id) == ids::air) kn->dst = (int) zen::Dst::air;
            fxKnobs.push_back (std::move (kn));
        }
        delaySync.addItemList (ZenProcessor::delaySyncNames(), 1);
        addAndMakeVisible (delaySync);
        delaySync.setBounds (372, 392, 92, 24);
        comboAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, ids::delaySync, delaySync));
        pageComps[fx].push_back (&delaySync);
        pageLabels.push_back ({ fx, "CHORUS", { 356, 252 } });
        pageLabels.push_back ({ fx, "VOICE + OUTPUT", { 604, 252 } });
        pageLabels.push_back ({ fx, "ECHO", { 480, 362 } });
        pageLabels.push_back ({ fx, "sync", { 418, 430 } });
        pageLabels.push_back ({ fx, "SHIMMER REVERB", { 490, 472 } });
    }

    // ---- PLAY page ------------------------------------------------------------------------
    {
        auto combo = [this] (juce::ComboBox& cb, const juce::StringArray& items, const char* id, juce::Rectangle<int> r)
        {
            cb.addItemList (items, 1);
            addAndMakeVisible (cb);
            cb.setBounds (r);
            comboAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, id, cb));
            pageComps[play].push_back (&cb);
        };
        auto toggle = [this] (SteelButton& b, const char* id, juce::Point<float> pos)
        {
            b.setClickingTogglesState (true);
            addAndMakeVisible (b);
            place (b, pos, 70.0f, 54.0f, 0.0f, 23.0f);
            buttonAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, id, b));
            pageComps[play].push_back (&b);
        };

        combo (rootBox, ZenProcessor::rootNames(), ids::root, { 296, 278, 64, 24 });
        combo (scaleBox, ZenProcessor::scaleNames(), ids::scale, { 370, 278, 130, 24 });
        combo (octaveBox, { "oct -3", "oct -2", "oct -1", "oct 0", "oct +1", "oct +2", "oct +3" }, ids::octave, { 510, 278, 80, 24 });
        toggle (latchBtn, ids::latchTimbre, { 646.0f, 290.0f });

        toggle (arpOnBtn, ids::arpOn, { 244.0f, 392.0f });
        combo (arpModeBox, ZenProcessor::arpModeNames(), ids::arpMode, { 284, 380, 108, 24 });
        combo (arpRateBox, ZenProcessor::arpRateNames(), ids::arpRate, { 400, 380, 72, 24 });
        combo (arpOctBox, { "1 oct", "2 oct", "3 oct", "4 oct" }, ids::arpOctaves, { 480, 380, 76, 24 });
        addAndMakeVisible (arpGate);
        place (arpGate, { 620.0f, 386.0f }, 62.0f, 64.0f, 0.0f, 24.0f);
        arpGate.dia = 36.0f;
        sliderAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, ids::arpGate, arpGate));
        pageComps[play].push_back (&arpGate);
        toggle (arpHoldBtn, ids::arpHold, { 700.0f, 392.0f });

        toggle (padsBtn, ids::platesFromPads, { 330.0f, 502.0f });
        {
            juce::StringArray chans { "any ch" };
            for (int c = 1; c <= 16; ++c) chans.add ("ch " + juce::String (c));
            combo (padChanBox, chans, ids::padChannel, { 372, 490, 88, 24 });
        }
        for (auto* b : { &learnBtn, &resetPadsBtn })
        {
            addAndMakeVisible (b);
            pageComps[play].push_back (b);
        }
        learnBtn.ledColour = theme::red;
        place (learnBtn, { 534.0f, 502.0f }, 70.0f, 54.0f, 0.0f, 23.0f);
        place (resetPadsBtn, { 616.0f, 502.0f }, 70.0f, 54.0f, 0.0f, 23.0f);
        resetPadsBtn.showLed = false;
        learnBtn.onClick = [this]
        {
            learning = ! learning;
            learnPlate = -1;
            learnBtn.setToggleState (learning, juce::dontSendNotification);
            repaint (330, 600, 320, 60);
        };
        resetPadsBtn.onClick = [this] { proc.resetPadNotes(); };
        combo (bendBox, { "bend 1", "bend 2", "bend 3", "bend 4", "bend 5", "bend 7", "bend 12", "bend 24" }, ids::bendRange, { 664, 490, 84, 24 });
        combo (mpeBox, { "MPE off", "MPE, bend 12", "MPE, bend 24", "MPE, bend 48", "MPE, bend 96" }, ids::mpe, { 180, 490, 106, 24 });
        mpeBox.setTooltip ("MPE controllers: each note on its own channel (2-16) bends, presses and slides by itself. "
                           "Pads only play plates on the pad channel you pick while MPE is on.");

        pageLabels.push_back ({ play, "NOTE PLATES", { 443, 252 } });
        pageLabels.push_back ({ play, "ARPEGGIATOR", { 470, 350 } });
        pageLabels.push_back ({ play, "LAUNCHKEY PADS", { 470, 460 } });
        pageLabels.push_back ({ play, "PITCH BEND RANGE", { 706, 524 } });
        pageLabels.push_back ({ play, "MPE CONTROLLER", { 233, 524 } });
        pageLabels.push_back ({ play, "default: pads C1-B1 play plates 1-12, C2-D#2 play A-D", { 490, 562 } });
        pageLabels.push_back ({ play, "to remap: LEARN, click a plate, then hit a pad", { 490, 582 } });
    }

    // ---- WAVES page --------------------------------------------------------------------------
    addAndMakeVisible (wavePanel);
    wavePanel.setBounds (250, 240, 480, 336);
    wavePanel.setHint = [this] (const juce::String& t) { showHint (t); };
    pageComps[waves].push_back (&wavePanel);

    // ---- BEAT page ---------------------------------------------------------------------------
    addAndMakeVisible (beatPanel);
    beatPanel.setBounds (196, 240, 588, 340);
    beatPanel.setHint = [this] (const juce::String& t) { showHint (t); };
    pageComps[beat].push_back (&beatPanel);
    engineB.onScreenClick = [this] { showPage (waves); };

    for (int i = 0; i < numPages; ++i) tabs[(size_t) i]->setToggleState (i == 0, juce::dontSendNotification);
    showPage (synth);
    setSize (980, 980);
    lastTick = juce::Time::getMillisecondCounterHiRes();
    startTimerHz (60);
}

Content::~Content()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void Content::place (juce::Component& c, juce::Point<float> pos, float w, float h, float rotationDeg, float anchorY)
{
    const float ay = anchorY < 0.0f ? h * 0.5f : anchorY;
    c.setBounds (juce::roundToInt (pos.x - w * 0.5f), juce::roundToInt (pos.y - ay), juce::roundToInt (w), juce::roundToInt (h));
    if (rotationDeg != 0.0f)
        c.setTransform (juce::AffineTransform::rotation (juce::degreesToRadians (rotationDeg), pos.x, pos.y));
}

void Content::setupKnob (WoodKnob& k, const char* paramId, int dst)
{
    k.dst = dst;
    sliderAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, paramId, k));
}

void Content::setupSynthPage()
{
    addAndMakeVisible (porthole);
    porthole.setBounds (381, 381, 218, 218);
    pageComps[synth].push_back (&porthole);

    struct K { const char* label; const char* id; zen::Dst dst; };
    const K ks[8] = { { "CUTOFF", ids::cutoff, zen::Dst::cutoff }, { "RES", ids::res, zen::Dst::res }, { "DRIVE", ids::drive, zen::Dst::drive },
                      { "ENV", ids::envAmt, zen::Dst::none }, { "ATTACK", ids::ampA, zen::Dst::none }, { "DECAY", ids::ampD, zen::Dst::none },
                      { "SUSTAIN", ids::ampS, zen::Dst::none }, { "RELEASE", ids::ampR, zen::Dst::none } };
    for (int i = 0; i < 8; ++i)
    {
        mainKnobs[(size_t) i] = std::make_unique<WoodKnob> (ks[i].label, 42.0f);
        auto& k = *mainKnobs[(size_t) i];
        addAndMakeVisible (k);
        const float a = -66.0f + (float) i * 132.0f / 7.0f;
        place (k, pol (175.0f, a), 120.0f, 120.0f);
        k.labelDir = { std::sin (juce::degreesToRadians (a)), -std::cos (juce::degreesToRadians (a)) };
        if (i == 0 || i == 7) k.labelDir = { 0.0f, 0.82f };   // the two ends: label underneath, clear of the engine boxes
        setupKnob (k, ks[i].id, (int) ks[i].dst);
        pageComps[synth].push_back (&k);
    }
    mainKnobs[3]->setDoubleClickReturnValue (true, 0.5);

    for (int i = 0; i < numModButtons; ++i)
    {
        const int src = srcForButton (i);
        modButtons[(size_t) i] = std::make_unique<SteelButton> (ZenProcessor::srcShortNames()[src], 22.0f);
        auto& b = *modButtons[(size_t) i];
        b.ledColour = theme::srcColour (src);
        addAndMakeVisible (b);
        const float a = -60.0f + 120.0f / (float) (numModButtons - 1) * (float) i;
        place (b, pol (255.0f, a), 52.0f, 54.0f, a, 23.0f);
        b.onClick = [this, src] { arm (src); };
        b.setTooltip ("Click to assign " + ZenProcessor::srcNames()[src] + ": then drag any knob with a ring to set how much it moves it."
                      + (src == (int) zen::Src::slide ? juce::String (" (SLIDE is CC74: the up-down axis on an MPE controller)") : juce::String()));
        pageComps[synth].push_back (&b);
    }

    addAndMakeVisible (engineA); addAndMakeVisible (engineB);
    engineA.setBounds (120, 390, 185, 200);   // leaves room for the PITCH and MOD strips at the edges
    engineB.setBounds (675, 390, 185, 200);
    pageComps[synth].push_back (&engineA);
    pageComps[synth].push_back (&engineB);

    // quick-assign: when a source is armed, dragging a knob sets that source's amount on it
    auto hook = [this] (WoodKnob& k)
    {
        k.modDrag = [this, &k] (const juce::MouseEvent& e, int phase) -> bool
        {
            if (armedSrc == 0 || k.dst == 0) return false;
            static float startAmt = 0.0f;
            static int slot = -1;
            if (phase == 3)
            {
                const int s = proc.findSlot ((zen::Src) armedSrc, (zen::Dst) k.dst);
                if (s >= 0) { setParam (ids::slotSrc (s), 0.0f); setParam (ids::slotDst (s), 0.0f); setParam (ids::slotAmt (s), 0.0f); }
                return true;
            }
            if (phase == 0)
            {
                slot = proc.findOrMakeSlot ((zen::Src) armedSrc, (zen::Dst) k.dst);
                startAmt = slot >= 0 ? proc.apvts.getRawParameterValue (ids::slotAmt (slot))->load() : 0.0f;
                if (slot < 0) showHint ("all 12 mod slots are in use - clear one on the MOD page");
                return true;
            }
            if (slot >= 0 && phase == 1)
            {
                const float amt = juce::jlimit (-1.0f, 1.0f, startAmt + (float) (e.getDistanceFromDragStartX() - e.getDistanceFromDragStartY()) / 160.0f);
                setParam (ids::slotAmt (slot), amt);
            }
            return true;
        };
    };
    for (auto& k : mainKnobs) hook (*k);
    for (auto& k : engineA.knobs) hook (*k);
    for (auto& k : engineB.knobs) hook (*k);
}

void Content::setParam (const juce::String& id, float plainValue)
{
    if (auto* prm = proc.apvts.getParameter (id))
    {
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->convertTo0to1 (plainValue));
        prm->endChangeGesture();
    }
}

void Content::showHint (const juce::String& t)
{
    hint = t;
    hintTime = juce::Time::getMillisecondCounter();
    repaint (330, 600, 320, 60);
}

void Content::arm (int src)
{
    armedSrc = (armedSrc == src) ? 0 : src;
    hint = {};
    const auto col = theme::srcColour (armedSrc);
    auto mark = [&] (WoodKnob& k) { k.armedColour = (armedSrc != 0 && k.dst != 0) ? col : juce::Colours::transparentBlack; k.repaint(); };
    for (auto& k : mainKnobs) mark (*k);
    for (auto& k : engineA.knobs) mark (*k);
    for (auto& k : engineB.knobs) mark (*k);
    for (int i = 0; i < 4; ++i)
    {
        auto& pl = *plates[(size_t) (12 + i)];
        pl.armed = armedSrc == (int) zen::Src::plateA + i;
        pl.armedColour = col;
        pl.repaint();
    }
    if (armedSrc != 0)
    {
        if (page != synth) showPage (synth);
        hint = "drag a ringed knob to set how much " + ZenProcessor::srcNames()[armedSrc] + " moves it  -  double-click removes  -  click again when done";
    }
    repaint (0, 600, 980, 80);
}

void Content::showPage (int p)
{
    page = p;
    for (int i = 0; i < numPages; ++i)
    {
        tabs[(size_t) i]->setToggleState (i == p, juce::dontSendNotification);
        for (auto* c : pageComps[i]) c->setVisible (i == p || (i == synth && p == stage && c == &porthole));
    }
    for (int i = 0; i < zen::numSlots; ++i)
        slotRows[(size_t) i]->setVisible (p == mod && (i / 6) == slotPage);
    for (int i = 0; i < 2; ++i) slotPageBtns[(size_t) i]->setToggleState (i == slotPage, juce::dontSendNotification);
    if (p == stage) porthole.setBounds (300, 230, 380, 380);
    else porthole.setBounds (381, 381, 218, 218);
    if (p != synth && armedSrc != 0) arm (armedSrc);   // disarm
    repaint();
}

juce::String Content::dstShort (int d) const
{
    static const char* n[] = { "", "PITCH", "A TUNE", "DETUNE", "SHAPE", "A LEVEL", "MORPH", "WARP", "UNISON", "B LEVEL", "AIR",
                               "BLOOM", "RES", "DRIVE", "SWELL", "PAN", "LFO1", "LFO2", "CHORUS", "ECHOES", "SPACE" };
    return n[juce::jlimit (0, zen::numDst - 1, d)];
}

namespace
{
    std::map<int, juce::Image>& slabMemory()   // shared between editor openings
    {
        static std::map<int, juce::Image> m;
        return m;
    }
    juce::File slabFile (int pixels)
    {
        // bump the version when the wood or the lettering changes
        return ZenProcessor::dataFolder().getChildFile ("slab-v4-" + juce::String (pixels) + ".png");
    }
}

void Content::setRenderScale (float ppu)
{
   #if JUCE_ANDROID || JUCE_IOS
    const float cap = 3072.0f;
   #else
    const float cap = 2560.0f;
   #endif
    const int want = (int) juce::jlimit (640.0f, cap, std::round (980.0f * ppu / 128.0f) * 128.0f);
    if (want == (int) slabPixels && slab.isValid()) return;
    slabPixels = (float) want;
    ++renderGeneration;

    auto& mem = slabMemory();
    if (auto it = mem.find (want); it != mem.end()) { slab = it->second; repaint(); return; }

    const auto file = slabFile (want);
    if (file.existsAsFile())
    {
        auto img = juce::ImageFileFormat::loadFrom (file);
        if (img.isValid() && img.getWidth() == want)
        {
            slab = mem[want] = img.convertedToFormat (juce::Image::ARGB);
            repaint();
            return;
        }
    }

    if (! slab.isValid())
    {
        finishSlab (wood::render (want), want);   // nothing to show yet: draw it now
        return;
    }

    // keep showing the current wood (a little soft) while the sharper one renders
    const int gen = renderGeneration;
    juce::Thread::launch ([safe = juce::Component::SafePointer<Content> (this), want, gen]
    {
        auto img = wood::render (want);
        juce::MessageManager::callAsync ([safe, img, want, gen]
        {
            if (safe != nullptr && safe->renderGeneration == gen) safe->finishSlab (img, want);
        });
    });
}

void Content::finishSlab (juce::Image img, int want)
{
    // static burned lettering, baked into the slab
    {
        juce::Graphics g (img);
        g.addTransform (juce::AffineTransform::scale ((float) want / 980.0f));
        theme::arcText (g, "zenthesizer", centre, 368.0f, 0.0f, theme::font (theme::FontKind::serif, 44.0f));
        theme::arcText (g, "TOUCH  SYNTHESIZER", centre, 386.0f, juce::degreesToRadians (-56.0f), theme::font (theme::FontKind::sans, 11.0f, 0.24f));
        theme::arcText (g, "VISUALS", centre, 392.0f, juce::degreesToRadians (60.0f), theme::font (theme::FontKind::sans, 11.0f, 0.24f));
        theme::arcText (g, "TIMBRE", centre, 250.0f, juce::MathConstants<float>::pi, theme::font (theme::FontKind::sans, 11.0f, 0.24f), theme::burn, true);
    }
    auto& mem = slabMemory();
    if (mem.size() > 2) mem.erase (mem.begin());
    mem[want] = img;
    slab = img;
    repaint();

    // save a copy for next time, off the message thread
    juce::Thread::launch ([img, want]
    {
        const auto file = slabFile (want);
        if (! file.getParentDirectory().createDirectory()) return;
        const auto tmp = file.getSiblingFile (file.getFileName() + ".part");
        tmp.deleteFile();
        {
            juce::FileOutputStream os (tmp);
            if (! os.openedOk() || ! juce::PNGImageFormat().writeImageToStream (img, os)) return;
        }
        tmp.moveFileTo (file);
    });
}

void Content::paint (juce::Graphics& g)
{
    if (slab.isValid())
        g.drawImage (slab, 0, 0, 980, 980, 0, 0, slab.getWidth(), slab.getHeight());
    else
    {
        g.fillAll (theme::table);
        g.setColour (juce::Colour (0xffc79865));
        g.fillEllipse (40.0f, 40.0f, 900.0f, 900.0f);
    }

    const auto clip = g.getClipBounds();

    // page headings
    for (auto& pl : pageLabels)
        if (pl.page == page)
        {
            const juce::Rectangle<float> r (pl.pos.x - 220.0f, pl.pos.y - 8.0f, 440.0f, 16.0f);
            if (clip.intersects (r.toNearestInt()))
                theme::burnText (g, pl.text, r, juce::Justification::centred, theme::font (theme::FontKind::sansBold, 11.5f, 0.2f));
        }

    // readout under the porthole, or the current hint
    const juce::Rectangle<int> readoutArea (330, 600, 320, 60);
    if (clip.intersects (readoutArea) && page != stage)
    {
        juce::String text = hint;
        if (text.isEmpty() && learning)
            text = learnPlate < 0 ? "LEARN: click a plate, then hit a pad" : "now hit the pad for plate " + plates[(size_t) learnPlate]->label;
        if (text.isEmpty() && page == synth)
        {
            const float hz = proc.display().cutoffHzNow.load();
            const int voices = proc.display().activeVoices.load();
            const auto pitchName = juce::MidiMessage::getMidiNoteName (juce::roundToInt (proc.display().lastPitch.load()), true, true, 3);
            text = (hz < 1000.0f ? juce::String (juce::roundToInt (hz)) + " Hz" : juce::String (hz / 1000.0f, 1) + " kHz")
                 + "   -   " + juce::String (voices) + (voices == 1 ? " voice" : " voices") + "   -   " + pitchName;
        }
        if (text.isNotEmpty())
        {
            const bool longText = text.length() > 48;
            theme::arcText (g, text, centre, longText ? 132.0f : 148.0f, juce::MathConstants<float>::pi,
                            theme::font (theme::FontKind::sans, longText ? 9.5f : 10.5f, 0.12f), theme::burn, true, false);
        }
    }

    // preset name along the bottom
    const juce::Rectangle<int> presetArea (340, 830, 300, 56);
    if (clip.intersects (presetArea))
    {
        const auto name = proc.currentPresetName();
        theme::arcText (g, name, centre, 374.0f, juce::MathConstants<float>::pi, theme::font (theme::FontKind::serif, 26.0f), theme::burn, true);
    }
}

void Content::mouseDown (const juce::MouseEvent& e)
{
    if (e.eventComponent == &presetHit) return;   // the preset menu opens on release (see mouseUp)
    if (e.eventComponent == this && armedSrc != 0) arm (armedSrc);   // click on bare wood: done assigning
}

void Content::mouseUp (const juce::MouseEvent& e)
{
    if (e.eventComponent == &presetHit && presetHit.getLocalBounds().contains (e.getPosition()) && ! e.mouseWasDraggedSinceMouseDown())
        showPresetMenu();
}

// Factory presets live in folders by category ("World/Asia" is a folder inside a folder).
void Content::showPresetMenu()
{
    enum { idSave = 900, idOpening = 901, idOpeningReset = 902, idUser = 1000, idDelete = 2000 };
    const auto info = ZenProcessor::presetInfo();
    const auto current = proc.currentPresetName();

    struct Folder { juce::String name; juce::PopupMenu menu; bool ticked = false; std::vector<int> order; };
    std::vector<Folder> folders;   // top level, in first-seen order
    std::map<juce::String, std::vector<std::pair<juce::String, juce::PopupMenu>>> subs;   // "World" -> [("Asia", menu)]
    std::map<juce::String, bool> subTicked;
    auto folderFor = [&folders] (const juce::String& n) -> Folder&
    {
        for (auto& f : folders) if (f.name == n) return f;
        folders.push_back ({ n, {}, false, {} });
        return folders.back();
    };

    juce::PopupMenu menu;
    for (int i = 0; i < (int) info.size(); ++i)
    {
        const auto& pi = info[(size_t) i];
        const bool on = pi.name == current;
        if (pi.category == "Init") continue;
        const auto top = pi.category.upToFirstOccurrenceOf ("/", false, false);
        const auto sub = pi.category.fromFirstOccurrenceOf ("/", false, false);
        auto& f = folderFor (top);
        if (on) f.ticked = true;
        if (sub.isEmpty()) { f.menu.addItem (i + 1, pi.name, true, on); continue; }
        auto& list = subs[top];
        auto it = std::find_if (list.begin(), list.end(), [&sub] (auto& pr) { return pr.first == sub; });
        if (it == list.end()) { list.push_back ({ sub, {} }); it = std::prev (list.end()); }
        it->second.addItem (i + 1, pi.name, true, on);
        if (on) subTicked[top + "/" + sub] = true;
    }
    menu.addSectionHeader ("Presets");
    for (auto& f : folders)
    {
        for (auto& [subName, subMenu] : subs[f.name])
            f.menu.addSubMenu (subName, subMenu, true, nullptr, subTicked[f.name + "/" + subName]);
        menu.addSubMenu (f.name, f.menu, true, nullptr, f.ticked);
    }
    menu.addItem (1, "Init", true, current == "Init");

    const auto user = ZenProcessor::userPresets();
    juce::PopupMenu mine, del;
    for (int i = 0; i < user.size(); ++i)
    {
        mine.addItem (idUser + i, user[i].getFileNameWithoutExtension(), true, user[i].getFileNameWithoutExtension() == current);
        del.addItem (idDelete + i, user[i].getFileNameWithoutExtension());
    }
    menu.addSeparator();
    menu.addSubMenu ("Your presets", mine, user.size() > 0);
    menu.addItem (idSave, "Save current sound as...");
    if (user.size() > 0) menu.addSubMenu ("Delete one of yours", del);

    menu.addSeparator();
    const auto opening = proc.openingSoundName();
    menu.addSectionHeader ("Opening sound: " + (opening.isNotEmpty() ? opening : juce::String ("Temple Mist (default)")));
    menu.addItem (idOpening, "Open new Zenthesizers with this sound (" + current + ")");
    menu.addItem (idOpeningReset, "Back to the default opening sound", opening.isNotEmpty());

    showMenu (menu, presetHit, [safe = juce::Component::SafePointer<Content> (this), user] (int r)
    {
        if (safe == nullptr || r <= 0) return;
        auto& self = *safe;
        if (r == idSave) { self.savePresetAs(); return; }
        if (r == idOpening)
        {
            self.showHint (self.proc.setOpeningSound() ? "new Zenthesizers will open with " + self.proc.openingSoundName()
                                                       : "couldn't save the opening sound");
            return;
        }
        if (r == idOpeningReset) { self.proc.clearOpeningSound(); self.showHint ("new Zenthesizers open with Temple Mist"); return; }
        if (r >= idDelete)
        {
            if (juce::isPositiveAndBelow (r - idDelete, user.size())) user[r - idDelete].deleteFile();
            self.showHint ("deleted");
            return;
        }
        if (r >= idUser)
        {
            if (juce::isPositiveAndBelow (r - idUser, user.size())) self.proc.loadUserPreset (user[r - idUser]);
        }
        else self.proc.setCurrentProgram (r - 1);
        self.repaint();
    });
}

void Content::savePresetAs()
{
    auto* w = new juce::AlertWindow ("Save preset", "Name this sound (your waves are saved with it):", juce::MessageBoxIconType::NoIcon);
    w->addTextEditor ("name", proc.currentPresetName() + " 2");
    w->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    w->enterModalState (true, juce::ModalCallbackFunction::create ([safe = juce::Component::SafePointer<Content> (this), w] (int r)
    {
        if (safe == nullptr || r != 1) return;
        const auto name = w->getTextEditorContents ("name");
        safe->showHint (safe->proc.saveUserPreset (name) ? "saved " + name.trim() : "couldn't save that preset");
        safe->repaint (340, 830, 300, 56);
    }), true);
}

void Content::stepPreset (int delta)
{
    const int n = proc.getNumPrograms();
    proc.setCurrentProgram ((proc.getCurrentProgram() + delta + n) % n);
    repaint (340, 830, 300, 56);
}

void Content::updateModRings()
{
    std::array<std::vector<ModArc>, zen::numDst> arcs;
    for (int i = 0; i < zen::numSlots; ++i)
    {
        const int s = (int) std::lround (proc.apvts.getRawParameterValue (ids::slotSrc (i))->load());
        const int d = (int) std::lround (proc.apvts.getRawParameterValue (ids::slotDst (i))->load());
        const float a = proc.apvts.getRawParameterValue (ids::slotAmt (i))->load();
        if (s == 0 || d == 0) continue;
        arcs[(size_t) d].push_back ({ theme::srcColour (s), d == (int) zen::Dst::aTune ? a * 0.5f : a });
    }
    auto apply = [&] (WoodKnob& k)
    {
        if (k.dst <= 0) return;
        k.setArcs (arcs[(size_t) k.dst]);
        k.setLive (proc.display().dstNorm[(size_t) k.dst].load());
    };
    for (auto& k : mainKnobs) apply (*k);
    for (auto& k : engineA.knobs) apply (*k);
    for (auto& k : engineB.knobs) apply (*k);
    for (auto& k : fxKnobs) apply (*k);

    // timbre plates say what they do
    for (int i = 0; i < 4; ++i)
    {
        juce::String sub;
        for (int sI = 0; sI < zen::numSlots && sub.isEmpty(); ++sI)
            if ((int) std::lround (proc.apvts.getRawParameterValue (ids::slotSrc (sI))->load()) == (int) zen::Src::plateA + i)
                sub = dstShort ((int) std::lround (proc.apvts.getRawParameterValue (ids::slotDst (sI))->load()));
        auto& pl = *plates[(size_t) (12 + i)];
        if (pl.subLabel != sub) { pl.subLabel = sub; pl.repaint(); }
    }
    // note plates: the note they play (or, on PLAY, the pad that plays them)
    const int root = (int) std::lround (param (ids::root)), sc = (int) std::lround (param (ids::scale)), oct = (int) std::lround (param (ids::octave)) - 3;
    for (int i = 0; i < zen::numPlates; ++i)
    {
        juce::String sub;
        if (page == play)
        {
            const int n = proc.getPadNote (i);
            sub = n >= 0 ? "pad " + juce::MidiMessage::getMidiNoteName (n, true, true, 3) : "no pad";
        }
        else if (i < zen::numNotePlates) sub = juce::MidiMessage::getMidiNoteName (zen::plateNote (i, root, sc, oct), true, true, 3);
        else continue;
        auto& pl = *plates[(size_t) i];
        if (pl.subLabel != sub) { pl.subLabel = sub; pl.repaint(); }
    }
}

void Content::timerCallback()
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    const float dt = (float) juce::jlimit (0.0, 0.1, (now - lastTick) * 0.001);
    lastTick = now;
    const int vis = juce::jlimit (0, 2, proc.visualsMode.load());
    static int frame = 0;
    ++frame;
    const bool lowFrame = vis == 2 || (frame % 2 == 0);   // LOW and OFF run the visuals at 30 fps

    for (int i = 0; i < 3; ++i)
        if (visButtons[(size_t) i]->getToggleState() != (i == vis)) visButtons[(size_t) i]->setToggleState (i == vis, juce::dontSendNotification);

    if (porthole.isVisible() && lowFrame) porthole.tick (dt * (vis == 2 ? 1.0f : 2.0f), vis);
    if (page == synth && lowFrame)
    {
        engineA.tick (dt * (vis == 2 ? 1.0f : 2.0f), vis > 0);
        engineB.tick (dt * (vis == 2 ? 1.0f : 2.0f), vis > 0);
    }

    const auto& d = proc.display();

    pitchStrip.tick (dt);
    pitchStrip.setShown (d.bendNow.load());
    modStrip.setShown (d.src[(int) zen::Src::wheel].load());

    // looper lights: red recording, amber overdubbing, green playing; the ring is the loop's position
    {
        auto& lp = proc.looper();
        const int st = lp.displayState.load();
        const float pos = lp.displayPos.load();
        const juce::Colour col = (st == zen::Looper::recording || st == zen::Looper::closing || st == zen::Looper::armed) ? theme::red
                               : st == zen::Looper::overdubbing ? theme::amber
                               : st == zen::Looper::playing ? theme::green : theme::ledOff;
        const bool blink = (st == zen::Looper::recording && std::fmod (now * 0.001, 0.5) < 0.25)
                        || ((st == zen::Looper::armed || st == zen::Looper::closing) && std::fmod (now * 0.001, 0.25) < 0.125);   // waiting for the bar
        loopBtn.ledColour = col;
        loopBtn.setLedLevel (st == zen::Looper::empty || st == zen::Looper::stopped ? 0.0f : (blink ? 0.5f : 1.0f));
        loopBtn.setRing ((st == zen::Looper::empty || st == zen::Looper::armed) ? -1.0f : (st == zen::Looper::stopped ? 0.0f : pos),
                         st == zen::Looper::stopped ? theme::ledOff : col);
        loopUndoBtn.ledColour = theme::amber;
        loopUndoBtn.setLedLevel (lp.displayCanUndo.load() ? 0.6f : 0.0f);
        if (std::abs (loopLevelKnob.getValue() - proc.loopLevel.load()) > 1.0e-4 && ! loopLevelKnob.isMouseButtonDown())
            loopLevelKnob.setValue (proc.loopLevel.load(), juce::dontSendNotification);
    }

    for (int i = 0; i < zen::numPlates; ++i)
    {
        auto& pl = *plates[(size_t) i];
        pl.setLevel (d.plateLevel[(size_t) i].load());
        if (const bool lk = d.plateLocked[(size_t) i].load(); lk != pl.locked) { pl.locked = lk; pl.repaint(); }
        if (vis > 0 && lowFrame) pl.tick (dt * (vis == 2 ? 1.0f : 2.0f));
        const bool wantArmed = (learning && learnPlate == i) || (armedSrc == (int) zen::Src::plateA + (i - 12) && i >= 12);
        if (pl.armed != wantArmed) { pl.armed = wantArmed; pl.armedColour = learning ? theme::red : theme::srcColour (armedSrc); pl.repaint(); }
    }

    if (page == synth)
        for (int i = 0; i < numModButtons; ++i)
        {
            const int src = srcForButton (i);
            float v = std::abs (d.src[(size_t) src].load());
            if (armedSrc == src) v = 0.65f + 0.35f * std::sin ((float) now * 0.008f);
            modButtons[(size_t) i]->setLedLevel (v);
        }

    if (frame % 3 == 0) updateModRings();

    // pad learn
    if (learning && learnPlate >= 0)
    {
        const int cnt = d.inCount.load();
        if (cnt != lastInCount)
        {
            proc.setPadNote (learnPlate, d.inNote.load());
            learnPlate = -1;
            repaint (330, 600, 320, 60);
        }
    }

    if (hint.isNotEmpty() && armedSrc == 0 && hintTime != 0 && juce::Time::getMillisecondCounter() - hintTime > 6000)
    {
        hint.clear(); hintTime = 0;
        repaint (330, 600, 320, 60);
    }

    if (frame % 10 == 0)
    {
        repaint (330, 600, 320, 60);       // readout
        repaint (340, 830, 300, 56);       // preset name (host may change it)
    }
}

//==============================================================================
ZenEditor::ZenEditor (ZenProcessor& p) : AudioProcessorEditor (p), content (p)
{
    addAndMakeVisible (content);
    setResizable (true, true);
   #if JUCE_ANDROID || JUCE_IOS
    // the app fills the screen; the slab is centred and can be zoomed
    constrainer.setSizeLimits (200, 200, 8000, 8000);
    for (auto* b : { &midiBtn, &audioBtn })
    {
        b->showLed = false;
        b->labelColour = theme::bone;
        addAndMakeVisible (b);
    }
    midiBtn.onClick = []
    {
        if (juce::BluetoothMidiDevicePairingDialogue::isAvailable())
            juce::BluetoothMidiDevicePairingDialogue::open();
    };
    audioBtn.onClick = []
    {
        if (auto* holder = juce::StandalonePluginHolder::getInstance())
            holder->showAudioSettingsDialog();
    };
    fitBtn.showLed = false;
    fitBtn.labelColour = theme::bone;
    addChildComponent (fitBtn);
    fitBtn.onClick = [this] { zoom = 1.0f; applyZoom (true); };
    juce::Desktop::getInstance().addGlobalMouseListener (&gestures);
    content.showHint ("two fingers on the wood to zoom  -  double-tap the wood to zoom in  -  FIT zooms out");
   #else
    constrainer.setFixedAspectRatio (1.0);
    constrainer.setSizeLimits (560, 560, 1400, 1400);
    for (auto* b : { &fullBtn, &returnBtn }) { b->showLed = false; b->labelColour = theme::bone; }
    addAndMakeVisible (fullBtn);
    addChildComponent (returnBtn);
    fullBtn.setTooltip ("Fill the screen (Esc comes back)");
    fullBtn.onClick = [this] { setFullscreen (true); };
    returnBtn.onClick = [this] { setFullscreen (false); };
   #endif
    setConstrainer (&constrainer);
    setSize (860, 860);
}

void ZenEditor::paint (juce::Graphics& g)
{
    g.fillAll (theme::table);
   #if ! (JUCE_ANDROID || JUCE_IOS)
    if (full != nullptr)
        theme::burnText (g, "Zenthesizer is fullscreen  -  press Esc or BACK there, or tap below",
                         getLocalBounds().toFloat().withSizeKeepingCentre ((float) getWidth() - 40.0f, 30.0f).translated (0.0f, -50.0f),
                         juce::Justification::centred, theme::font (theme::FontKind::sans, 14.0f, 0.1f), theme::bone);
   #endif
}

ZenEditor::~ZenEditor()
{
   #if JUCE_ANDROID || JUCE_IOS
    juce::Desktop::getInstance().removeGlobalMouseListener (&gestures);
    WoodTouch::fingersOnWood = 0;
   #endif
}

void ZenEditor::resized()
{
   #if ! (JUCE_ANDROID || JUCE_IOS)
    fullBtn.setBounds (8, 8, 58, 52);
    returnBtn.setBounds (getLocalBounds().withSizeKeepingCentre (150, 70).translated (0, 10));
    if (full != nullptr) return;   // the slab is in the fullscreen window
   #endif
    const float side = (float) std::min (getWidth(), getHeight());
    const float oldBase = baseScale;
    baseScale = side / (float) designSize;
    if (zoom <= 1.001f)
        origin = { ((float) getWidth() - side) * 0.5f, ((float) getHeight() - side) * 0.5f };
    else
        origin = origin * (baseScale / std::max (0.0001f, oldBase));

   #if JUCE_ANDROID || JUCE_IOS
    // the device buttons sit in the screen's top corners, outside the round slab
    const float bs = std::max (0.75f, baseScale);
    const int bw = juce::roundToInt (64.0f * bs), bh = juce::roundToInt (58.0f * bs);
    midiBtn.dome = audioBtn.dome = 26.0f * bs;
    midiBtn.setBounds (6, 6, bw, bh);
    audioBtn.setBounds (getWidth() - 6 - bw, 6, bw, bh);
    fitBtn.dome = 26.0f * bs;
    fitBtn.setBounds (6, getHeight() - 6 - bh, bw, bh);
   #endif
    applyZoom (true);
}

void ZenEditor::clampOrigin()
{
    const float size = (float) designSize * baseScale * zoom;
    auto axis = [] (float o, float extent, float avail)
    {
        if (extent <= avail) return (avail - extent) * 0.5f;
        return juce::jlimit (avail - extent, 0.0f, o);
    };
    origin = { axis (origin.x, size, (float) getWidth()), axis (origin.y, size, (float) getHeight()) };
}

void ZenEditor::applyZoom (bool sharpen)
{
    clampOrigin();
    const float k = baseScale * zoom;
    content.setTransform (juce::AffineTransform::scale (k).translated (origin));
   #if JUCE_ANDROID || JUCE_IOS
    midiBtn.toFront (false);
    audioBtn.toFront (false);
    fitBtn.setVisible (zoom > 1.01f);
    fitBtn.toFront (false);
   #endif
    if (sharpen)
    {
        float physical = 1.0f;
        if (auto* disp = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay()) physical = (float) disp->scale;
        content.setRenderScale (k * std::max (1.0f, physical));
    }
}

// Zoom so that the slab point `anchorContent` (in 0..980 units) sits under `editorPos`.
void ZenEditor::zoomAround (juce::Point<float> editorPos, float newZoom, juce::Point<float> anchorContent)
{
    zoom = juce::jlimit (1.0f, 4.0f, newZoom);
    origin = editorPos - anchorContent * (baseScale * zoom);
}

#if ! (JUCE_ANDROID || JUCE_IOS)
ZenEditor::FullWindow::FullWindow (ZenEditor& e) : ed (e)
{
    setOpaque (true);
    setWantsKeyboardFocus (true);
    backBtn.showLed = false;
    backBtn.labelColour = theme::bone;
    backBtn.setTooltip ("Back to the plugin window (Esc)");
    backBtn.onClick = [this] { juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<ZenEditor> (&ed)] { if (safe != nullptr) safe->setFullscreen (false); }); };
    addAndMakeVisible (backBtn);
}

void ZenEditor::FullWindow::resized()
{
    const float side = (float) std::min (getWidth(), getHeight());
    const float k = side / (float) designSize;
    ed.content.setTransform (juce::AffineTransform::scale (k).translated (((float) getWidth() - side) * 0.5f, ((float) getHeight() - side) * 0.5f));
    float physical = 1.0f;
    if (auto* disp = juce::Desktop::getInstance().getDisplays().getDisplayForRect (getScreenBounds())) physical = (float) disp->scale;
    ed.content.setRenderScale (k * std::max (1.0f, physical));
    const int bw = juce::roundToInt (70.0f * std::max (1.0f, k)), bh = juce::roundToInt (64.0f * std::max (1.0f, k));
    backBtn.dome = 26.0f * std::max (1.0f, k);
    backBtn.setBounds (getWidth() - bw - 12, 12, bw, bh);
    backBtn.toFront (false);
}

bool ZenEditor::FullWindow::keyPressed (const juce::KeyPress& k)
{
    if (k == juce::KeyPress::escapeKey) { backBtn.onClick(); return true; }
    return false;
}

void ZenEditor::setFullscreen (bool on)
{
    if (on == (full != nullptr)) return;
    if (on)
    {
        const auto* disp = juce::Desktop::getInstance().getDisplays().getDisplayForRect (getScreenBounds());
        if (disp == nullptr) disp = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay();
        if (disp == nullptr) return;
        full = std::make_unique<FullWindow> (*this);
        full->setBounds (disp->logicalBounds.toNearestInt());
        full->addAndMakeVisible (content);            // moves the slab out of the plugin window
        content.setBounds (0, 0, designSize, designSize);
        full->resized();
        full->setName ("Zenthesizer");
        full->addToDesktop (juce::ComponentPeer::windowAppearsOnTaskbar);
        full->setVisible (true);
        full->toFront (true);
        full->grabKeyboardFocus();
        fullBtn.setVisible (false);
        returnBtn.setVisible (true);
    }
    else
    {
        addAndMakeVisible (content);                  // back into the plugin window
        content.setBounds (0, 0, designSize, designSize);
        full.reset();
        fullBtn.setVisible (true);
        returnBtn.setVisible (false);
        resized();
        fullBtn.toFront (false);
    }
    repaint();
}
#endif

#if JUCE_ANDROID || JUCE_IOS
// Zoom only ever comes from bare wood: two fingers on the wood pinch, one finger on the wood
// drags (when zoomed), two quick taps on the wood zoom in. Touches on plates, knobs and buttons
// never zoom, and FIT (bottom left) zooms back out.
void ZenEditor::Gestures::beginPan (int index)
{
    if (auto it = touches.find (index); it != touches.end())
    {
        panStartPos = it->second;
        panStartOrigin = ed.origin;
    }
}

void ZenEditor::Gestures::mouseDown (const juce::MouseEvent& e)
{
    if (! e.source.isTouch()) return;
    const int idx = e.source.getIndex();
    const auto pos = ed.getLocalPoint (nullptr, e.getScreenPosition()).toFloat();
    touches[idx] = pos;

    if (e.eventComponent != &ed.content)
    {
        lastTap = 0;              // playing something in between cancels a double-tap
        return;
    }

    wood.insert (idx);
    downTime[idx] = juce::Time::getMillisecondCounter();
    downPos[idx] = pos;
    moved = false;

    if (wood.size() >= 2 && ! pinching && ! pinchPending)
    {
        // two fingers on the wood: a pinch, once they actually spread or close a little
        pinchA = *wood.begin();
        pinchB = *std::next (wood.begin());
        startDist = std::max (20.0f, touches[pinchA].getDistanceFrom (touches[pinchB]));
        pinchPending = true;
        lastTap = 0;
        return;
    }
    if (wood.size() == 1) beginPan (idx);
}

void ZenEditor::Gestures::mouseDrag (const juce::MouseEvent& e)
{
    if (! e.source.isTouch()) return;
    const int idx = e.source.getIndex();
    if (wood.count (idx) == 0) return;
    const auto pos = ed.getLocalPoint (nullptr, e.getScreenPosition()).toFloat();
    touches[idx] = pos;
    if (pos.getDistanceFrom (downPos[idx]) > 12.0f) moved = true;

    if ((pinchPending || pinching) && touches.count (pinchA) != 0 && touches.count (pinchB) != 0)
    {
        const auto a = touches[pinchA], b = touches[pinchB];
        const float d = std::max (20.0f, a.getDistanceFrom (b));
        if (pinchPending)
        {
            if (std::abs (d / startDist - 1.0f) < 0.08f) return;   // not a pinch yet
            pinchPending = false;
            pinching = true;
            WoodTouch::fingersOnWood = 2;
            startDist = d;
            startZoom = ed.zoom;
            anchor = ((a + b) * 0.5f - ed.origin) / (ed.baseScale * ed.zoom);
        }
        ed.zoomAround ((a + b) * 0.5f, startZoom * d / startDist, anchor);
        ed.applyZoom (false);
    }
    else if (! pinching && ! pinchPending && wood.size() == 1 && ed.zoom > 1.01f)
    {
        ed.origin = panStartOrigin + (pos - panStartPos);
        ed.applyZoom (false);
    }
}

void ZenEditor::Gestures::mouseUp (const juce::MouseEvent& e)
{
    if (! e.source.isTouch()) return;
    const int idx = e.source.getIndex();
    const bool wasWood = wood.count (idx) != 0;
    const auto pos = touches.count (idx) != 0 ? touches[idx] : juce::Point<float>();
    touches.erase (idx);
    wood.erase (idx);

    if ((pinching || pinchPending) && (idx == pinchA || idx == pinchB))
    {
        const bool didZoom = pinching;
        pinching = pinchPending = false;
        WoodTouch::fingersOnWood = 0;
        if (! wood.empty()) beginPan (*wood.begin());   // keep dragging with the finger that's left
        if (didZoom) ed.applyZoom (true);               // re-render the wood sharp for this zoom
        return;
    }

    // a quick, still tap on the wood; two of them in a row zoom in (never out: that's FIT)
    if (wasWood && wood.empty() && ! moved)
    {
        const auto now = juce::Time::getMillisecondCounter();
        if (now - downTime[idx] < 250)
        {
            if (lastTap != 0 && now - lastTap < 350 && pos.getDistanceFrom (lastTapPos) < 60.0f)
            {
                if (ed.zoom < 1.5f)
                {
                    ed.zoomAround (pos, 2.5f, (pos - ed.origin) / (ed.baseScale * ed.zoom));
                    ed.applyZoom (true);
                }
                lastTap = 0;
            }
            else
            {
                lastTap = now;
                lastTapPos = pos;
            }
        }
    }
    if (wasWood && moved && wood.empty() && ed.zoom > 1.01f) ed.applyZoom (true);
}
#endif
