#pragma once

// The BEAT page: an organic drum kit with a 16-step pattern. It runs independently of the
// synth presets, plays after the looper, and keeps the looper on the bar while it runs.

#include "../PluginProcessor.h"
#include "Widgets.h"
#include <map>

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
        // on a patch row TUNE picks the note the patch was captured at; elsewhere it's +/- 12 semitones
        tune.textFromValueFunction  = [this] (double v)
        {
            if (isPatchRow (selected))
                return juce::MidiMessage::getMidiNoteName (ZenProcessor::noteForTune ((float) v), true, true, 3);
            const int st = juce::roundToInt ((v - 0.5) * 24.0);
            return (st > 0 ? "+" : "") + juce::String (st) + " st";
        };
        decay.textFromValueFunction = [] (double v) { return "x" + juce::String (std::pow (4.0, (v - 0.5) * 2.0), 2); };

        auto edit = [this] (std::function<void (zen::DrumSettings&)> fn)
        {
            d = proc.getDrums(); fn (d); proc.setDrums (d);
        };
        tempo.onValueChange  = [this, edit] { edit ([this] (auto& x) { x.tempo = (float) tempo.getValue(); }); repaint (infoArea()); };
        swing.onValueChange  = [this, edit] { edit ([this] (auto& x) { x.swing = (float) swing.getValue(); }); };
        volume.onValueChange = [this, edit] { edit ([this] (auto& x) { x.volume = (float) volume.getValue(); }); };
        level.onValueChange  = [this, edit] { edit ([this] (auto& x) { x.level[(size_t) selected] = (float) level.getValue(); }); };
        tune.onValueChange   = [this, edit] { edit ([this] (auto& x) { x.tune[(size_t) selected] = (float) tune.getValue(); }); d = proc.getDrums(); repaint (labelArea()); };
        decay.onValueChange  = [this, edit] { edit ([this] (auto& x) { x.decay[(size_t) selected] = (float) decay.getValue(); }); };
        for (auto* k : { &tempo, &swing, &volume, &level, &tune, &decay })
        {
            addAndMakeVisible (k);
            k->onDragEnd = [this] { proc.hitDrum (selected); };
        }
        tempo.onDragEnd = swing.onDragEnd = volume.onDragEnd = nullptr;
        tune.onDragEnd = [this]
        {
            if (! isPatchRow (selected)) { proc.hitDrum (selected); return; }
            // a patch row: capture the patch again at the new note, then let you hear it
            proc.recaptureDrumPatch (selected);
            hearWhenReady = selected;
            if (setHint) setHint ("capturing " + proc.drumRowName (selected).toLowerCase() + " at "
                                  + juce::MidiMessage::getMidiNoteName (ZenProcessor::noteForTune ((float) tune.getValue()), true, true, 3));
        };

        previewBtn.showLed = false;
        previewBtn.setTriggeredOnMouseDown (true);
        previewBtn.onClick = [this] { proc.hitDrum (selected); };
        addAndMakeVisible (previewBtn);

        for (auto* b : { &soundBtn, &kitsBtn }) { b->showLed = false; addAndMakeVisible (b); }
        soundBtn.onClick = [this] { showSounds(); };
        kitsBtn.onClick  = [this] { showKits(); };

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
        // bottom row: the selected row's sound
        previewBtn.setBounds (152, 274, 58, 56);
        soundBtn.setBounds (210, 274, 60, 56);
        level.setBounds (270, 270, 66, 66);
        tune.setBounds (336, 270, 66, 66);
        decay.setBounds (402, 270, 66, 66);
        kitsBtn.setBounds (474, 274, 58, 56);
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
            const auto name = rowNames[(size_t) r];
            const float fsz = name.length() > 10 ? 8.5f : (name.length() > 8 ? 9.5f : 10.5f);
            theme::burnText (g, name, { -30.0f, y, grid.getX() - 10.0f + 30.0f, ch }, juce::Justification::centredRight,
                             theme::font (sel ? theme::FontKind::sansBold : theme::FontKind::sans, fsz, 0.10f),
                             sel ? theme::burn : theme::burnSoft);
            if (isPatchRow (r))   // a small brass dot: this row is a captured synth patch
            {
                g.setColour (theme::brass.withAlpha (0.9f));
                g.fillEllipse (grid.getX() - 7.0f, y + ch * 0.5f - 2.0f, 4.0f, 4.0f);
            }
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
        const auto selName = rowNames[(size_t) selected];
        theme::burnText (g, selName, { 20.0f, 284.0f, 128.0f, 16.0f }, juce::Justification::centredRight,
                         theme::font (theme::FontKind::sansBold, selName.length() > 10 ? 9.5f : 11.5f, 0.14f));
        const bool busy = proc.capturesRunning.load() > 0;
        const juce::String sub = busy ? "capturing..."
                               : isPatchRow (selected) ? "patch at " + juce::MidiMessage::getMidiNoteName (ZenProcessor::noteForTune (d.tune[(size_t) selected]), true, true, 3)
                                                       : "row " + juce::String (selected + 1);
        theme::burnText (g, sub,
                         { 20.0f, 302.0f, 128.0f, 14.0f }, juce::Justification::centredRight,
                         theme::font (theme::FontKind::sans, 9.0f, 0.14f), busy ? theme::burn : theme::burnSoft);
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
                previewBtn { "HEAR", 20.0f }, soundBtn { "SOUND", 20.0f }, kitsBtn { "KITS", 20.0f };
    std::array<juce::String, zen::numDrums> rowNames;
    int hearWhenReady = -1;
    bool groovesBringKit = true;
    bool wasBusy = false;

    bool isPatchRow (int r) const { return d.sound[(size_t) r] == (int) zen::Sound::patch; }

    void refreshNames()
    {
        bool changed = false;
        for (int r = 0; r < zen::numDrums; ++r)
        {
            auto n = proc.drumRowName (r);
            if (n != rowNames[(size_t) r]) { rowNames[(size_t) r] = n; changed = true; }
        }
        if (changed) repaint();
    }
    WoodKnob tempo { "TEMPO", 34.0f }, swing { "SWING", 34.0f }, volume { "DRUMS", 34.0f },
             level { "LEVEL", 34.0f }, tune { "TUNE", 34.0f }, decay { "DECAY", 34.0f };

    juce::Rectangle<float> gridArea() const { return { 104.0f, 82.0f, 416.0f, 176.0f }; }
    juce::Rectangle<int> infoArea() const   { return { 90, 60, 420, 16 }; }
    juce::Rectangle<int> labelArea() const  { return { 0, 280, 150, 40 }; }

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
        tune.updateText();
        refreshNames();
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
        enum { idKitToggle = 1000 };
        juce::PopupMenu m;
        m.addSectionHeader ("Load a groove (replaces the pattern, tempo and swing)");
        const auto& gs = zen::grooves();
        std::map<juce::String, juce::PopupMenu> folders;
        juce::StringArray folderOrder;
        for (int i = 0; i < (int) gs.size(); ++i)
        {
            const auto& g = gs[(size_t) i];
            const auto label = juce::String (g.name) + "   (" + juce::String ((int) g.bpm) + ")";
            const juce::String folder (g.folder);
            if (folder.isEmpty()) { m.addItem (i + 1, label); continue; }
            folderOrder.addIfNotAlreadyThere (folder);
            folders[folder].addItem (i + 1, label);
        }
        m.addSeparator();
        for (auto& f : folderOrder) m.addSubMenu (f, folders[f]);
        m.addSeparator();
        m.addItem (idKitToggle, "Switch to the kit each groove was written for (rows with synth patches stay)", true, groovesBringKit);
        showMenu (m, grooveBtn,
                         [safe = juce::Component::SafePointer<BeatPanel> (this)] (int r)
                         {
                             if (safe == nullptr || r <= 0) return;
                             if (r == idKitToggle) { safe->groovesBringKit = ! safe->groovesBringKit; return; }
                             const auto& g = zen::grooves()[(size_t) (r - 1)];
                             safe->proc.loadGroove (r - 1);
                             juce::String kitNote;
                             if (juce::String (g.kit).isNotEmpty())
                                 for (int k = 0; k < (int) zen::kits().size(); ++k)
                                     if (juce::String (zen::kits()[(size_t) k].name) == g.kit)
                                     {
                                         if (safe->groovesBringKit) { safe->proc.loadKit (k, true); kitNote = "  (" + juce::String (g.kit) + " kit)"; }
                                         else kitNote = "  -  written for the " + juce::String (g.kit) + " kit";
                                     }
                             safe->syncFromProcessor();
                             if (safe->setHint) safe->setHint (juce::String (g.name) + " loaded" + kitNote + "  -  tap PLAY");
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

        refreshNames();
        const bool busy = proc.capturesRunning.load() > 0;
        if (busy != wasBusy)
        {
            wasBusy = busy;
            repaint (labelArea());
            if (! busy && hearWhenReady >= 0)
            {
                proc.hitDrum (hearWhenReady);   // the fresh capture is in: play it once
                if (setHint) setHint (proc.drumRowName (hearWhenReady).toLowerCase() + " is on its row  -  TUNE picks the note");
                hearWhenReady = -1;
            }
        }
    }

    //==========================================================================
    enum MenuIds { idLibrary = 1, idFactory = 1000, idUser = 3000, idNow = 5000 };

    void showSounds()
    {
        const int row = selected;
        juce::PopupMenu m;
        m.addSectionHeader ("Sound for row " + juce::String (row + 1));
        const int cur = d.sound[(size_t) row];
        for (auto& grp : zen::soundGroups())
        {
            juce::PopupMenu sub;
            bool has = false;
            for (int s : grp.sounds) { sub.addItem (idLibrary + s, juce::String (zen::soundName (s)).toLowerCase(), true, s == cur); has |= s == cur; }
            m.addSubMenu (grp.name, sub, true, nullptr, has);
        }
        m.addSeparator();
        m.addSectionHeader ("A synth patch, one note per hit");

        const auto info = ZenProcessor::presetInfo();
        juce::StringArray order;
        for (auto& i : info) if (! order.contains (i.category) && i.category != "Init") order.add (i.category);
        // folders as in the preset menu ("World/Asia" sits inside "World")
        juce::PopupMenu factory;
        juce::StringArray tops;
        for (auto& c : order) tops.addIfNotAlreadyThere (c.upToFirstOccurrenceOf ("/", false, false));
        for (auto& top : tops)
        {
            juce::PopupMenu topMenu;
            for (auto& c : order)
            {
                if (c.upToFirstOccurrenceOf ("/", false, false) != top) continue;
                const auto subName = c.fromFirstOccurrenceOf ("/", false, false);
                juce::PopupMenu items;
                for (int i = 0; i < (int) info.size(); ++i)
                    if (info[(size_t) i].category == c) items.addItem (idFactory + i, info[(size_t) i].name);
                if (subName.isEmpty()) topMenu = items;
                else topMenu.addSubMenu (subName, items);
            }
            factory.addSubMenu (top, topMenu);
        }
        m.addSubMenu ("From a preset", factory);

        const auto user = ZenProcessor::userPresets();
        juce::PopupMenu mine;
        for (int i = 0; i < user.size(); ++i) mine.addItem (idUser + i, user[i].getFileNameWithoutExtension());
        m.addSubMenu ("From your presets", mine, user.size() > 0);
        m.addItem (idNow, "The sound you're playing now (" + proc.currentPresetName() + ")");

        showMenu (m, soundBtn,
                         [safe = juce::Component::SafePointer<BeatPanel> (this), row, info, user] (int r)
                         {
                             if (safe == nullptr || r <= 0) return;
                             auto& self = *safe;
                             if (r < idFactory)
                             {
                                 self.proc.setDrumSound (row, r - idLibrary);
                                 self.afterSoundChange (row);
                                 self.proc.hitDrum (row);
                                 return;
                             }
                             ZenProcessor::PatchRecipe recipe;
                             if (r >= idNow)       recipe = self.proc.snapshotOfCurrentSound();
                             else if (r >= idUser) { recipe.kind = 2; recipe.file = user[r - idUser].getFullPathName(); recipe.name = user[r - idUser].getFileNameWithoutExtension(); }
                             else                  { recipe.kind = 1; recipe.name = info[(size_t) (r - idFactory)].name; }
                             self.capturePatch (row, std::move (recipe));
                         });
    }

    void capturePatch (int row, ZenProcessor::PatchRecipe recipe)
    {
        // start each new patch on C3 with a natural length; TUNE moves the note afterwards
        auto x = proc.getDrums();
        if (x.sound[(size_t) row] != (int) zen::Sound::patch)
        {
            x.tune[(size_t) row] = ZenProcessor::tuneForNote (48);
            x.decay[(size_t) row] = 0.5f;
            proc.setDrums (x);
        }
        const auto name = recipe.name;
        proc.captureDrumPatch (row, std::move (recipe));
        hearWhenReady = row;
        afterSoundChange (row);
        if (setHint) setHint ("capturing " + name + " onto row " + juce::String (row + 1) + "...");
    }

    void afterSoundChange (int row)
    {
        d = proc.getDrums();
        selectRow (row);
    }

    void showKits()
    {
        juce::PopupMenu m;
        m.addSectionHeader ("Load a kit (all 8 rows; your pattern stays)");
        const auto& ks = zen::kits();
        for (int i = 0; i < (int) ks.size(); ++i) m.addItem (i + 1, ks[(size_t) i].name);
        showMenu (m, kitsBtn,
                         [safe = juce::Component::SafePointer<BeatPanel> (this)] (int r)
                         {
                             if (safe == nullptr || r <= 0) return;
                             safe->proc.loadKit (r - 1);
                             safe->afterSoundChange (safe->selected);
                             if (safe->setHint) safe->setHint (juce::String (zen::kits()[(size_t) (r - 1)].name) + " kit loaded");
                         });
    }
};
