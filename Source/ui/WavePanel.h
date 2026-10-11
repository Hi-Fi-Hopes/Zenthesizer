#pragma once

// The WAVES page: the wavetable sandbox for Engine B's own table (OWN).
// Draw a cycle or shape its harmonics, build up to 8 keyframes that POSITION morphs through,
// start from shapes or built-in tables, import audio, and keep a library of your waves.

#include "../PluginProcessor.h"
#include "Widgets.h"

class WavePanel : public juce::Component, private juce::Timer
{
public:
    explicit WavePanel (ZenProcessor& p) : proc (p)
    {
        setPaintingIsUnclipped (true);
        for (auto* t : { &drawTab, &harmTab })
        {
            addAndMakeVisible (t);
            t->onClick = [this, t] { harmonicsMode = (t == &harmTab); refreshHarmonics(); updateTabs(); repaint(); };
        }
        updateTabs();
        harmTab.setComponentID ("harmTab");

        for (auto* b : { &shapesBtn, &smoothBtn, &addBtn, &removeBtn, &importBtn, &loadBtn, &saveBtn })
        {
            b->showLed = false;
            addAndMakeVisible (b);
        }
        shapesBtn.onClick = [this] { showShapesMenu(); };
        smoothBtn.onClick = [this] { edit ([] (zen::sandbox::Cycle& c) { zen::sandbox::smooth (c); }); };
        addBtn.onClick    = [this] { addFrame(); };
        removeBtn.onClick = [this] { removeFrame(); };
        importBtn.onClick = [this] { importFile(); };
        loadBtn.onClick   = [this] { showLibraryMenu(); };
        saveBtn.onClick   = [this] { saveToLibrary(); };

        kf = proc.getKeyframes();
        version = proc.keyframesVersion();
        startTimerHz (30);
    }

    std::function<void (const juce::String&)> setHint;

    void resized() override
    {
        const float w = (float) getWidth();
        drawTab.setBounds ((int) (w * 0.5f) - 116, 4, 108, 30);
        harmTab.setBounds ((int) (w * 0.5f) + 8, 4, 108, 30);
        screen = { 20.0f, 46.0f, w - 40.0f, 160.0f };
        strip  = { 30.0f, 216.0f, w - 60.0f, 44.0f };
        const float step = (w - 20.0f) / 7.0f;
        int i = 0;
        for (auto* b : { &shapesBtn, &smoothBtn, &addBtn, &removeBtn, &importBtn, &loadBtn, &saveBtn })
            b->setBounds ((int) (10.0f + step * (float) i++), 272, (int) step, 52);
    }

    void paint (juce::Graphics& g) override
    {
        // carved glass screen
        g.setColour (juce::Colour (0x59ffecc8));
        g.fillRoundedRectangle (screen.expanded (4.0f).translated (0, 1.0f), 18.0f);
        g.setColour (juce::Colour (0x8c3b1f0c));
        g.fillRoundedRectangle (screen.expanded (3.0f), 18.0f);
        g.setColour (theme::glass);
        g.fillRoundedRectangle (screen, 16.0f);
        {
            juce::Graphics::ScopedSaveState ss (g);
            juce::Path clip; clip.addRoundedRectangle (screen, 16.0f);
            g.reduceClipRegion (clip);
            paintScreen (g);
            juce::ColourGradient sh (juce::Colours::black.withAlpha (0.7f), screen.getX(), screen.getY(),
                                     juce::Colours::transparentBlack, screen.getX(), screen.getY() + 12.0f, false);
            g.setGradientFill (sh);
            g.fillRect (screen);
        }
        paintStrip (g);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (WoodTouch::claims (e)) return;   // part of a pinch on the wood
        const auto p = e.position;
        if (strip.contains (p))
        {
            const int i = frameAt (p.x);
            if (i >= 0 && i < (int) kf.size()) { current = i; refreshHarmonics(); repaint(); }
            return;
        }
        if (! screen.contains (p)) return;
        dragging = true;
        last = toUnit (p);
        if (harmonicsMode)
        {
            refMax = harm.maxOf (numBars);
            setBar (last);
        }
        else
        {
            edit ([this] (zen::sandbox::Cycle& c) { zen::sandbox::drawLine (c, last.x, last.y, last.x, last.y); }, false);
        }
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! dragging) return;
        const auto u = toUnit (e.position);
        if (harmonicsMode)
        {
            // sweep across bars: set every bar between the last point and this one
            const int b0 = barAt (last.x), b1 = barAt (u.x);
            const int step = b1 >= b0 ? 1 : -1;
            for (int b = b0; b != b1 + step; b += step)
            {
                const float t = b1 == b0 ? 1.0f : (float) (b - b0) / (float) (b1 - b0);
                setBarValue (b, last.y + (u.y - last.y) * t);
            }
            kf[(size_t) current] = harm.toCycle();
            pushSoon();
        }
        else
        {
            const auto a = last;
            edit ([a, u] (zen::sandbox::Cycle& c) { zen::sandbox::drawLine (c, a.x, a.y, u.x, u.y); }, false);
        }
        last = u;
        repaint();
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (! dragging) return;
        dragging = false;
        push();
    }

private:
    ZenProcessor& proc;
    ZenProcessor::Keyframes kf;
    zen::sandbox::Harmonics harm;
    int current = 0, version = -1;
    bool harmonicsMode = false, dragging = false, pushPending = false;
    juce::uint32 lastPush = 0;
    float refMax = 1.0f;
    juce::Point<float> last;
    juce::Rectangle<float> screen, strip;
    std::unique_ptr<juce::FileChooser> chooser;
    static constexpr int numBars = 32;

    TabButton drawTab { "DRAW" }, harmTab { "HARMONICS" };
    SteelButton shapesBtn { "SHAPES", 20.0f }, smoothBtn { "SMOOTH", 20.0f }, addBtn { "+ FRAME", 20.0f },
                removeBtn { "- FRAME", 20.0f }, importBtn { "IMPORT", 20.0f }, loadBtn { "LOAD", 20.0f }, saveBtn { "SAVE", 20.0f };

    void updateTabs()
    {
        drawTab.setToggleState (! harmonicsMode, juce::dontSendNotification);
        harmTab.setToggleState (harmonicsMode, juce::dontSendNotification);
    }

    juce::Point<float> toUnit (juce::Point<float> p) const
    {
        const auto r = screen.reduced (10.0f, 14.0f);
        return { juce::jlimit (0.0f, 1.0f, (p.x - r.getX()) / r.getWidth()),
                 juce::jlimit (-1.0f, 1.0f, 1.0f - 2.0f * (p.y - r.getY()) / r.getHeight()) };
    }

    int barAt (float ux) const { return juce::jlimit (1, numBars, 1 + (int) (ux * (float) numBars)); }
    void setBar (juce::Point<float> u) { setBarValue (barAt (u.x), u.y); kf[(size_t) current] = harm.toCycle(); pushSoon(); repaint(); }
    void setBarValue (int b, float y)
    {
        // y -1..1 -> 0..1 of the loudest bar (drawn on a gentle curve, so quiet partials are easy to set)
        const float v = juce::jlimit (0.0f, 1.0f, (y + 1.0f) * 0.5f);
        harm.mag[(size_t) b] = v * v * refMax;
    }

    int frameAt (float x) const
    {
        const int n = (int) kf.size();
        const float slot = strip.getWidth() / (float) zen::maxKeyframes;
        const float x0 = strip.getCentreX() - slot * (float) n * 0.5f;
        return (int) std::floor ((x - x0) / slot);
    }

    void refreshHarmonics()
    {
        current = juce::jlimit (0, (int) kf.size() - 1, current);
        harm = zen::sandbox::Harmonics::of (kf[(size_t) current]);
    }

    template <typename Fn>
    void edit (Fn fn, bool pushNow = true)
    {
        fn (kf[(size_t) current]);
        refreshHarmonics();
        listenToOwn();
        if (pushNow) push(); else pushSoon();
        repaint();
    }

    void listenToOwn()
    {
        // editing the sandbox: switch Engine B to OWN so you hear it
        if (auto* prm = proc.apvts.getParameter (ids::bTable))
            if ((int) std::lround (proc.apvts.getRawParameterValue (ids::bTable)->load()) != zen::userTable)
            {
                prm->beginChangeGesture(); prm->setValueNotifyingHost (prm->convertTo0to1 ((float) zen::userTable)); prm->endChangeGesture();
            }
        if (proc.apvts.getRawParameterValue (ids::bLevel)->load() < 0.05f && setHint)
            setHint ("Engine B is turned down - raise B LEVEL on the SYNTH page to hear your waves");
    }

    void push()
    {
        proc.setKeyframes (kf);
        version = proc.keyframesVersion();
        pushPending = false;
        lastPush = juce::Time::getMillisecondCounter();
    }
    void pushSoon() { pushPending = true; }

    void timerCallback() override
    {
        if (pushPending && juce::Time::getMillisecondCounter() - lastPush > 60) push();
        if (! dragging && proc.keyframesVersion() != version)
        {
            kf = proc.getKeyframes();
            version = proc.keyframesVersion();
            refreshHarmonics();
            repaint();
        }
        if (isShowing()) repaint (strip.expanded (6.0f).toNearestInt());   // live position marker
    }

    //==========================================================================
    void paintScreen (juce::Graphics& g)
    {
        const auto r = screen.reduced (10.0f, 14.0f);
        g.setColour (theme::glow.withAlpha (0.08f));
        g.drawHorizontalLine ((int) r.getCentreY(), r.getX(), r.getRight());
        const auto& c = kf[(size_t) current];

        auto wavePath = [&] (const zen::sandbox::Cycle& cyc)
        {
            juce::Path p;
            const int n = 256;
            for (int i = 0; i < n; ++i)
            {
                const float x = r.getX() + r.getWidth() * (float) i / (float) (n - 1);
                const float v = cyc[(size_t) (i * (zen::tableSize - 1) / (n - 1))];
                const float y = r.getCentreY() - v * r.getHeight() * 0.5f;
                if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
            }
            return p;
        };

        if (harmonicsMode)
        {
            g.setColour (theme::glow.withAlpha (0.18f));
            g.strokePath (wavePath (c), juce::PathStrokeType (1.4f));
            const float m = dragging ? refMax : harm.maxOf (numBars);
            const float bw = r.getWidth() / (float) numBars;
            for (int b = 1; b <= numBars; ++b)
            {
                const float v = std::sqrt (juce::jlimit (0.0f, 1.0f, harm.mag[(size_t) b] / m));
                const float h = v * r.getHeight();
                const auto bar = juce::Rectangle<float> (r.getX() + bw * (float) (b - 1) + 2.0f, r.getBottom() - h, bw - 4.0f, h);
                juce::ColourGradient gr (theme::brassHi, bar.getX(), bar.getY(), theme::brassLo, bar.getX(), r.getBottom(), false);
                g.setGradientFill (gr);
                g.fillRoundedRectangle (bar, 2.0f);
                if (b == 1 || b % 4 == 0)
                {
                    g.setColour (theme::glow.withAlpha (0.45f));
                    g.setFont (theme::font (theme::FontKind::sans, 9.0f));
                    g.drawText (juce::String (b), juce::Rectangle<float> (bar.getX() - 4.0f, r.getBottom() + 1.0f, bw + 8.0f, 11.0f),
                                juce::Justification::centred, false);
                }
            }
        }
        else
        {
            auto p = wavePath (c);
            g.setColour (theme::glow.withAlpha (0.22f));
            g.strokePath (p, juce::PathStrokeType (7.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour (theme::glow);
            g.strokePath (p, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        g.setColour (theme::glow.withAlpha (0.5f));
        g.setFont (theme::font (theme::FontKind::sans, 10.0f, 0.12f));
        g.drawText ("KEYFRAME " + juce::String (current + 1) + " / " + juce::String ((int) kf.size())
                        + (harmonicsMode ? "   -   drag the bars" : "   -   draw with your finger or mouse"),
                    screen.reduced (14.0f, 6.0f).removeFromTop (14.0f), juce::Justification::centredLeft, false);
    }

    void paintStrip (juce::Graphics& g)
    {
        const int n = (int) kf.size();
        const float slot = strip.getWidth() / (float) zen::maxKeyframes;
        const float x0 = strip.getCentreX() - slot * (float) n * 0.5f;
        for (int i = 0; i < n; ++i)
        {
            const auto cell = juce::Rectangle<float> (x0 + slot * (float) i + 3.0f, strip.getY(), slot - 6.0f, strip.getHeight());
            g.setColour (juce::Colour (0x8c3b1f0c));
            g.fillRoundedRectangle (cell.expanded (2.0f), 9.0f);
            g.setColour (theme::glass);
            g.fillRoundedRectangle (cell, 8.0f);
            juce::Path p;
            const auto rr = cell.reduced (5.0f, 7.0f);
            for (int k = 0; k < 128; ++k)
            {
                const float v = kf[(size_t) i][(size_t) (k * (zen::tableSize - 1) / 127)];
                const float x = rr.getX() + rr.getWidth() * (float) k / 127.0f, y = rr.getCentreY() - v * rr.getHeight() * 0.5f;
                if (k == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
            }
            g.setColour (theme::glow.withAlpha (i == current ? 1.0f : 0.55f));
            g.strokePath (p, juce::PathStrokeType (i == current ? 1.6f : 1.1f));
            if (i == current)
            {
                g.setColour (theme::brass);
                g.drawRoundedRectangle (cell.expanded (1.0f), 8.0f, 2.0f);
            }
            theme::burnText (g, juce::String (i + 1), { cell.getX(), cell.getBottom() + 2.0f, cell.getWidth(), 12.0f },
                             juce::Justification::centredTop, theme::font (theme::FontKind::sans, 9.5f));
        }

        // where POSITION is right now, if OWN is playing
        if ((int) std::lround (proc.apvts.getRawParameterValue (ids::bTable)->load()) == zen::userTable && n > 1)
        {
            float pos = proc.display().dstNorm[(int) zen::Dst::bPos].load();
            if (pos < 0.0f) pos = proc.apvts.getRawParameterValue (ids::bPos)->load();
            const float x = x0 + slot * 0.5f + pos * slot * (float) (n - 1);
            g.setColour (theme::amber.withAlpha (0.35f));
            g.fillRoundedRectangle (x - 4.0f, strip.getY() - 6.0f, 8.0f, strip.getHeight() + 12.0f, 4.0f);
            g.setColour (theme::amber);
            g.fillRoundedRectangle (x - 1.0f, strip.getY() - 6.0f, 2.0f, strip.getHeight() + 12.0f, 1.0f);
        }
    }

    //==========================================================================
    void addFrame()
    {
        if ((int) kf.size() >= zen::maxKeyframes) { if (setHint) setHint ("8 keyframes is the limit"); return; }
        kf.insert (kf.begin() + current + 1, kf[(size_t) current]);
        ++current;
        refreshHarmonics(); listenToOwn(); push(); repaint();
        if (setHint) setHint ("keyframe copied - reshape it, and POSITION will morph between them");
    }

    void removeFrame()
    {
        if (kf.size() <= 1) return;
        kf.erase (kf.begin() + current);
        current = std::min (current, (int) kf.size() - 1);
        refreshHarmonics(); push(); repaint();
    }

    void showShapesMenu()
    {
        using namespace zen::sandbox;
        juce::PopupMenu m, builtIn;
        m.addSectionHeader ("Replace this keyframe with");
        for (int i = 0; i < (int) Shape::count; ++i) m.addItem (100 + i, shapeName ((Shape) i));
        for (int t = 0; t < zen::numTables; ++t)
            builtIn.addItem (200 + t, juce::String (zen::Wavetables::tableName (t)) + " (all 8 frames)");
        m.addSeparator();
        m.addSubMenu ("Copy a built-in table", builtIn);
        m.addItem (300, "Reset to one sine");
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&shapesBtn),
                         [safe = juce::Component::SafePointer<WavePanel> (this)] (int r)
                         {
                             if (safe == nullptr || r == 0) return;
                             auto& self = *safe;
                             if (r >= 100 && r < 200)
                                 self.edit ([r] (Cycle& c) { c = make ((Shape) (r - 100), juce::Random::getSystemRandom().nextInt() | 1); });
                             else if (r >= 200 && r < 300)
                             {
                                 self.kf.clear();
                                 for (int f = 0; f < zen::Wavetables::frames; ++f) self.kf.push_back (fromBuiltIn (r - 200, f));
                                 self.current = 0;
                                 self.refreshHarmonics(); self.listenToOwn(); self.push(); self.repaint();
                             }
                             else if (r == 300)
                             {
                                 self.kf = { make (Shape::sine) };
                                 self.current = 0;
                                 self.refreshHarmonics(); self.push(); self.repaint();
                             }
                         });
    }

    void importFile()
    {
        chooser = std::make_unique<juce::FileChooser> ("Import a waveform or wavetable", juce::File(), "*.wav;*.aif;*.aiff;*.flac;*.ogg");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [safe = juce::Component::SafePointer<WavePanel> (this)] (const juce::FileChooser& fc)
                              {
                                  if (safe == nullptr) return;
                                  const auto url = fc.getURLResult();
                                  if (url.isEmpty()) return;
                                  juce::String msg;
                                  auto stream = url.isLocalFile() ? std::unique_ptr<juce::InputStream> (url.getLocalFile().createInputStream())
                                                                  : url.createInputStream (juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress));
                                  if (safe->proc.importAudio (std::move (stream), msg)) { safe->current = 0; safe->listenToOwn(); }
                                  if (safe->setHint) safe->setHint (msg);
                              });
    }

    void showLibraryMenu()
    {
        const auto files = ZenProcessor::wavetableFolder().findChildFiles (juce::File::findFiles, false, "*.wav");
        juce::PopupMenu m;
        if (files.isEmpty()) m.addItem (-1, "(nothing saved yet - use SAVE)", false);
        for (int i = 0; i < files.size(); ++i) m.addItem (i + 1, files[i].getFileNameWithoutExtension());
       #if ! (JUCE_ANDROID || JUCE_IOS)
        m.addSeparator();
        m.addItem (10000, "Show the folder");
       #endif
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&loadBtn),
                         [safe = juce::Component::SafePointer<WavePanel> (this), files] (int r)
                         {
                             if (safe == nullptr || r == 0) return;
                             if (r == 10000)
                             {
                                 ZenProcessor::wavetableFolder().createDirectory();
                                 ZenProcessor::wavetableFolder().startAsProcess();
                                 return;
                             }
                             if (r < 1 || r > files.size()) return;
                             juce::String msg;
                             if (safe->proc.importAudio (std::unique_ptr<juce::InputStream> (files[r - 1].createInputStream()), msg))
                             {
                                 safe->current = 0;
                                 safe->listenToOwn();
                                 msg = "loaded " + files[r - 1].getFileNameWithoutExtension();
                             }
                             if (safe->setHint) safe->setHint (msg);
                         });
    }

    void saveToLibrary()
    {
        auto* w = new juce::AlertWindow ("Save your waves", "Name this wavetable:", juce::MessageBoxIconType::NoIcon);
        w->addTextEditor ("name", "My waves " + juce::String (ZenProcessor::wavetableFolder().getNumberOfChildFiles (juce::File::findFiles, "*.wav") + 1));
        w->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
        w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        w->enterModalState (true, juce::ModalCallbackFunction::create ([safe = juce::Component::SafePointer<WavePanel> (this), w] (int r)
        {
            if (safe == nullptr || r != 1) return;
            const auto name = juce::File::createLegalFileName (w->getTextEditorContents ("name").trim());
            if (name.isEmpty()) return;
            const auto f = ZenProcessor::wavetableFolder().getChildFile (name + ".wav");
            const bool ok = safe->proc.exportWavetable (f);
            if (safe->setHint) safe->setHint (ok ? "saved " + name + " (a standard wavetable WAV, 2048 samples per frame)" : "couldn't save that");
        }), true);
    }
};
