#pragma once

// The BEAT page: an organic drum kit with a 16-step pattern. It runs independently of the
// synth presets, plays after the looper, and keeps the looper on the bar while it runs.

#include "../PluginProcessor.h"
#include "Widgets.h"

class BeatPanel : public juce::Component, private juce::Timer
{
public:
    explicit BeatPanel (ZenProcessor& p) : proc (p)
    {
        setPaintingIsUnclipped (true);
        d = proc.getDrums();

        playBtn.setClickingTogglesState (true);
        playBtn.setTriggeredOnMouseDown (true);
        playBtn.ledColour = theme::green;
        playBtn.onClick = [this] { d = proc.getDrums(); d.playing = playBtn.getToggleState(); proc.setDrums (d); };
        for (auto* b : { &grooveBtn, &clearBtn }) b->showLed = false;
        grooveBtn.onClick = [this] { showGrooves(); };
        clearBtn.onClick  = [this] { d = proc.getDrums(); for (auto& row : d.steps) row.fill (0); proc.setDrums (d); repaint(); };
        for (auto* b : { &playBtn, &grooveBtn, &clearBtn }) addAndMakeVisible (b);

        tempo.setRange (50.0, 180.0, 1.0);
        tempo.textFromValueFunction = [] (double v) { return juce::String (juce::roundToInt (v)) + " BPM"; };
        swing.setRange (0.0, 1.0);
        swing.textFromValueFunction = [] (double v) { return juce::String (juce::roundToInt (v * 100.0)) + "%"; };
        volume.setRange (0.0, 1.0);
        volume.textFromValueFunction = swing.textFromValueFunction;
        for (auto* k : { &level, &tune, &decay })
        {
            k->setRange (0.0, 1.0);
            k->setDoubleClickReturnValue (true, 0.5);
        }
        level.textFromValueFunction = swing.textFromValueFunction;
        tune.textFromValueFunction  = [] (double v) { const int st = juce::roundToInt ((v - 0.5) * 24.0); return (st > 0 ? "+" : "") + juce::String (st) + " st"; };
        decay.textFromValueFunction = [] (double v) { return "x" + juce::String (std::pow (4.0, (v - 0.5) * 2.0), 2); };

        auto edit = [this] (std::function<void (zen::DrumSettings&)> fn)
        {
            d = proc.getDrums(); fn (d); proc.setDrums (d);
        };
        tempo.onValueChange  = [this, edit] { edit ([this] (auto& x) { x.tempo = (float) tempo.getValue(); }); repaint (infoArea()); };
        swing.onValueChange  = [this, edit] { edit ([this] (auto& x) { x.swing = (float) swing.getValue(); }); };
        volume.onValueChange = [this, edit] { edit ([this] (auto& x) { x.volume = (float) volume.getValue(); }); };
        level.onValueChange  = [this, edit] { edit ([this] (auto& x) { x.level[(size_t) selected] = (float) level.getValue(); }); };
        tune.onValueChange   = [this, edit] { edit ([this] (auto& x) { x.tune[(size_t) selected] = (float) tune.getValue(); }); };
        decay.onValueChange  = [this, edit] { edit ([this] (auto& x) { x.decay[(size_t) selected] = (float) decay.getValue(); }); };
        for (auto* k : { &tempo, &swing, &volume, &level, &tune, &decay })
        {
            addAndMakeVisible (k);
            k->onDragEnd = [this] { proc.hitDrum (selected); };
        }
        tempo.onDragEnd = swing.onDragEnd = volume.onDragEnd = nullptr;

        previewBtn.showLed = false;
        previewBtn.setTriggeredOnMouseDown (true);
        previewBtn.onClick = [this] { proc.hitDrum (selected); };
        addAndMakeVisible (previewBtn);

        syncFromProcessor();
        startTimerHz (30);
    }

    void resized() override
    {
        // top row: transport and feel
        playBtn.setBounds (96, 2, 60, 56);
        tempo.setBounds (160, 0, 70, 66);
        swing.setBounds (230, 0, 70, 66);
        volume.setBounds (300, 0, 70, 66);
        grooveBtn.setBounds (372, 2, 66, 56);
        clearBtn.setBounds (436, 2, 60, 56);
        // bottom row: the selected drum
        previewBtn.setBounds (196, 274, 60, 56);
        level.setBounds (260, 270, 70, 66);
        tune.setBounds (330, 270, 70, 66);
        decay.setBounds (400, 270, 70, 66);
    }

    void paint (juce::Graphics& g) override
    {
        const auto grid = gridArea();
        const float cw = grid.getWidth() / (float) zen::numSteps, ch = grid.getHeight() / (float) zen::numDrums;
        const int step = proc.display().drumStep.load();

        // beat groups: a faint burned bar over every 4 steps
        for (int b = 0; b < 4; ++b)
        {
            g.setColour (theme::burn.withAlpha (b % 2 == 0 ? 0.10f : 0.05f));
            g.fillRoundedRectangle (grid.getX() + cw * 4.0f * (float) b + 1.0f, grid.getY() - 4.0f, cw * 4.0f - 2.0f, grid.getHeight() + 8.0f, 8.0f);
        }
        // the playhead
        if (d.playing && step >= 0)
        {
            g.setColour (theme::amber.withAlpha (0.22f));
            g.fillRoundedRectangle (grid.getX() + cw * (float) step + 2.0f, grid.getY() - 4.0f, cw - 4.0f, grid.getHeight() + 8.0f, 6.0f);
        }

        for (int r = 0; r < zen::numDrums; ++r)
        {
            const float y = grid.getY() + ch * (float) r;
            // row name (tap to select and hear it)
            const bool sel = r == selected;
            theme::burnText (g, zen::drumName (r), { 0.0f, y, grid.getX() - 10.0f, ch }, juce::Justification::centredRight,
                             theme::font (sel ? theme::FontKind::sansBold : theme::FontKind::sans, 10.5f, 0.12f),
                             sel ? theme::burn : theme::burnSoft);
            if (sel)
            {
                g.setColour (theme::burn);
                g.fillRect (grid.getX() - 10.0f - 52.0f, y + ch - 4.0f, 52.0f, 1.5f);
            }
            for (int k = 0; k < zen::numSteps; ++k)
            {
                const auto c = juce::Point<float> (grid.getX() + cw * ((float) k + 0.5f), y + ch * 0.5f);
                const int v = d.steps[(size_t) r][(size_t) k];
                const bool now = d.playing && k == step && v > 0;
                if (v == 0)
                {
                    g.setColour (theme::burn.withAlpha (0.35f));
                    g.drawEllipse (c.x - 5.0f, c.y - 5.0f, 10.0f, 10.0f, 1.2f);
                    continue;
                }
                const float rad = v == 2 ? 8.0f : 6.0f;
                if (v == 2 || now)
                {
                    g.setColour (juce::Colour (0xffffc86e).withAlpha (now ? 0.7f : 0.35f));
                    g.fillEllipse (c.x - rad - 4.0f, c.y - rad - 4.0f, (rad + 4.0f) * 2.0f, (rad + 4.0f) * 2.0f);
                }
                juce::ColourGradient brass (theme::brassHi, c.x - rad * 0.4f, c.y - rad * 0.5f, theme::brassDk, c.x + rad, c.y + rad, true);
                brass.addColour (0.4, theme::brass);
                g.setGradientFill (brass);
                g.fillEllipse (c.x - rad, c.y - rad, rad * 2.0f, rad * 2.0f);
                g.setColour (juce::Colour (0x8c281608));
                g.drawEllipse (c.x - rad, c.y - rad, rad * 2.0f, rad * 2.0f, 1.0f);
            }
        }

        // tempo readout and the selected drum's name
        const float host = proc.hostTempo();
        const juce::String bpmText = host > 1.0f ? "following Ableton: " + juce::String (host, 1) + " BPM"
                                                 : juce::String (juce::roundToInt (d.tempo)) + " BPM";
        theme::burnText (g, bpmText, infoArea().toFloat(), juce::Justification::centred, theme::font (theme::FontKind::sans, 10.5f, 0.14f));
        theme::burnText (g, zen::drumName (selected), { 70.0f, 288.0f, 120.0f, 16.0f }, juce::Justification::centredRight,
                         theme::font (theme::FontKind::sansBold, 11.5f, 0.18f));
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (WoodTouch::claims (e)) return;
        const auto grid = gridArea();
        const float ch = grid.getHeight() / (float) zen::numDrums;
        if (e.position.y >= grid.getY() && e.position.y < grid.getBottom() && e.position.x < grid.getX())
        {
            selectRow ((int) ((e.position.y - grid.getY()) / ch));
            proc.hitDrum (selected);
            return;
        }
        int r, k;
        if (! cellAt (e.position, r, k)) return;
        d = proc.getDrums();
        const int v = d.steps[(size_t) r][(size_t) k];
        paintValue = (v + 1) % 3;             // tap cycles off -> on -> accent -> off
        paintRow = r;
        proc.setDrumStep (r, k, paintValue);
        d.steps[(size_t) r][(size_t) k] = (uint8_t) paintValue;
        if (paintValue > 0 && ! d.playing) proc.hitDrum (r);
        if (r != selected) selectRow (r);
        repaint();
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (paintRow < 0) return;
        int r, k;
        if (! cellAt (e.position, r, k) || r != paintRow) return;
        if (d.steps[(size_t) r][(size_t) k] == paintValue) return;
        proc.setDrumStep (r, k, paintValue);   // drag along a row to paint steps
        d.steps[(size_t) r][(size_t) k] = (uint8_t) paintValue;
        repaint();
    }

    void mouseUp (const juce::MouseEvent&) override { paintRow = -1; }

    std::function<void (const juce::String&)> setHint;

private:
    ZenProcessor& proc;
    zen::DrumSettings d;
    int selected = 0, paintRow = -1, paintValue = 1, lastStep = -2;

    SteelButton playBtn { "PLAY", 24.0f }, grooveBtn { "GROOVES", 20.0f }, clearBtn { "CLEAR", 20.0f },
                previewBtn { "HEAR", 20.0f };
    WoodKnob tempo { "TEMPO", 34.0f }, swing { "SWING", 34.0f }, volume { "DRUMS", 34.0f },
             level { "LEVEL", 34.0f }, tune { "TUNE", 34.0f }, decay { "DECAY", 34.0f };

    juce::Rectangle<float> gridArea() const { return { 104.0f, 82.0f, 416.0f, 176.0f }; }
    juce::Rectangle<int> infoArea() const   { return { 90, 60, 420, 16 }; }

    bool cellAt (juce::Point<float> p, int& r, int& k) const
    {
        const auto grid = gridArea();
        if (! grid.expanded (0.0f, 2.0f).contains (p)) return false;
        r = juce::jlimit (0, zen::numDrums - 1, (int) ((p.y - grid.getY()) / (grid.getHeight() / (float) zen::numDrums)));
        k = juce::jlimit (0, zen::numSteps - 1, (int) ((p.x - grid.getX()) / (grid.getWidth() / (float) zen::numSteps)));
        return true;
    }

    void selectRow (int r)
    {
        selected = juce::jlimit (0, zen::numDrums - 1, r);
        level.setValue (d.level[(size_t) selected], juce::dontSendNotification);
        tune.setValue (d.tune[(size_t) selected], juce::dontSendNotification);
        decay.setValue (d.decay[(size_t) selected], juce::dontSendNotification);
        repaint();
    }

    void syncFromProcessor()
    {
        d = proc.getDrums();
        playBtn.setToggleState (d.playing, juce::dontSendNotification);
        tempo.setValue (d.tempo, juce::dontSendNotification);
        swing.setValue (d.swing, juce::dontSendNotification);
        volume.setValue (d.volume, juce::dontSendNotification);
        selectRow (selected);
    }

    void showGrooves()
    {
        juce::PopupMenu m;
        m.addSectionHeader ("Load a groove (replaces the pattern, tempo and swing)");
        const auto& gs = zen::grooves();
        for (int i = 0; i < (int) gs.size(); ++i)
            m.addItem (i + 1, juce::String (gs[(size_t) i].name) + "   (" + juce::String ((int) gs[(size_t) i].bpm) + ")");
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&grooveBtn),
                         [safe = juce::Component::SafePointer<BeatPanel> (this)] (int r)
                         {
                             if (safe == nullptr || r <= 0) return;
                             safe->proc.loadGroove (r - 1);
                             safe->syncFromProcessor();
                             if (safe->setHint) safe->setHint (juce::String (zen::grooves()[(size_t) (r - 1)].name) + " loaded  -  tap PLAY");
                         });
    }

    void timerCallback() override
    {
        if (! isShowing()) return;
        // pick up changes made elsewhere (a session reload) without fighting a knob being dragged
        const auto fresh = proc.getDrums();
        if (fresh.playing != d.playing) playBtn.setToggleState (fresh.playing, juce::dontSendNotification);
        if (paintRow < 0) d = fresh;
        const int step = proc.display().drumStep.load();
        if (step != lastStep) { lastStep = step; repaint (gridArea().expanded (8.0f).toNearestInt()); }
        static float lastHost = -1.0f;
        if (std::abs (proc.hostTempo() - lastHost) > 0.05f) { lastHost = proc.hostTempo(); repaint (infoArea()); }
        playBtn.setLedLevel (d.playing ? (step % 4 == 0 ? 1.0f : 0.55f) : 0.0f);
    }
};
