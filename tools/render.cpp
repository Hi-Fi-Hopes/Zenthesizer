#include <typeinfo>
#include <dlfcn.h>
// Developer tool: plays a few notes, then renders each page of the editor to /tmp/zen_*.png.
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets.h"
#include "ui/Plank.h"

int main()
{
    std::setvbuf (stdout, nullptr, _IONBF, 0);
    juce::ScopedJuceInitialiser_GUI init;
    ZenProcessor proc;
    proc.setPlayConfigDetails (0, 2, 48000.0, 512);
    proc.prepareToPlay (48000.0, 512);

    // developer: dump every preset's engine settings (raw) for offline DSP analysis
    if (const char* dir = std::getenv ("ZEN_DUMPSETTINGS"))
    {
        static_assert (std::is_trivially_copyable<zen::Settings>::value, "Settings must be plain data");
        juce::File out (dir); out.createDirectory();
        juce::String names;
        for (int i = 0; i < proc.getNumPrograms(); ++i)
        {
            proc.setCurrentProgram (i);
            const auto st = proc.settingsSnapshot();
            out.getChildFile (juce::String (i).paddedLeft ('0', 3) + ".bin").replaceWithData (&st, sizeof (st));
            names << i << "\t" << proc.getProgramName (i) << "\t" << ZenProcessor::presetInfo()[(size_t) i].category << "\n";
        }
        out.getChildFile ("names.txt").replaceWithText (names);
        std::printf ("dumped %d presets (%d bytes each)\n", proc.getNumPrograms(), (int) sizeof (zen::Settings));
        return 0;
    }

    // every preset value must name a real parameter, and every preset must make sound
    int bad = 0;
    for (auto& pr : presets::all())
        for (auto& v : pr.values)
            if (proc.apvts.getParameter (v.id) == nullptr) { std::printf ("preset %s: unknown id %s\n", pr.name, v.id); ++bad; }
    std::printf ("%d presets, %d bad ids\n", (int) presets::all().size(), bad);
    juce::AudioBuffer<float> buf (2, 512);
    for (int i = 0; i < proc.getNumPrograms(); ++i)
    {
        proc.setCurrentProgram (i);
        proc.prepareToPlay (48000.0, 512);
        double sum = 0.0; float pk = 0.0f; bool finite = true;
        for (int b = 0; b < 190; ++b)
        {
            juce::MidiBuffer midi;
            if (b == 0) { midi.addEvent (juce::MidiMessage::noteOn (1, 57, (juce::uint8) 100), 0); midi.addEvent (juce::MidiMessage::noteOn (1, 64, (juce::uint8) 100), 0); }
            if (b == 150) { midi.addEvent (juce::MidiMessage::noteOff (1, 57), 0); midi.addEvent (juce::MidiMessage::noteOff (1, 64), 0); }
            proc.processBlock (buf, midi);
            for (int c = 0; c < 2; ++c)
                for (int k = 0; k < 512; ++k) { const float x = buf.getSample (c, k); if (! std::isfinite (x)) finite = false; sum += x * x; pk = std::max (pk, std::abs (x)); }
        }
        const float rms = (float) std::sqrt (sum / (190.0 * 1024.0));
        std::printf ("  %-22s rms %6.1f dB  peak %.2f %s\n", proc.getProgramName (i).toRawUTF8(), 20.0 * std::log10 (rms + 1e-9), pk, finite ? "" : "NaN!");
    }
    proc.setCurrentProgram (1);
    proc.prepareToPlay (48000.0, 512);
    for (int b = 0; b < 120; ++b)
    {
        juce::MidiBuffer midi;
        if (b == 0)
        {
            midi.addEvent (juce::MidiMessage::noteOn (10, 38, (juce::uint8) 110), 0);   // plate 3
            midi.addEvent (juce::MidiMessage::noteOn (10, 40, (juce::uint8) 90), 0);    // plate 5
            midi.addEvent (juce::MidiMessage::noteOn (10, 48, (juce::uint8) 90), 0);    // timbre A
            midi.addEvent (juce::MidiMessage::noteOn (1, 64, (juce::uint8) 100), 0);
        }
        if (b == 40) midi.addEvent (juce::MidiMessage::aftertouchChange (10, 38, 120), 0);
        if (b == 60) midi.addEvent (juce::MidiMessage::noteOn (10, 43, (juce::uint8) 100), 0);
        proc.processBlock (buf, midi);
    }

    // patch capture, end to end: a factory preset, a snapshot, and a state round trip
    {
        auto waitCaptures = [] (ZenProcessor& pr)
        {
            const double t = juce::Time::getMillisecondCounterHiRes();
            do juce::MessageManager::getInstance()->runDispatchLoopUntil (20);
            while ((pr.capturesRunning.load() > 0) && juce::Time::getMillisecondCounterHiRes() - t < 30000.0);
            return juce::Time::getMillisecondCounterHiRes() - t;
        };
        auto rowEnergy = [&buf] (ZenProcessor& pr, int row)
        {
            juce::MidiBuffer none;
            for (int b = 0; b < 400; ++b) pr.processBlock (buf, none);   // let the synth and any hit settle
            pr.hitDrum (row);
            double e = 0.0;
            for (int b = 0; b < 100; ++b)
            {
                pr.processBlock (buf, none);
                for (int k = 0; k < 512; ++k) e += buf.getSample (0, k) * buf.getSample (0, k);
            }
            return 10.0 * std::log10 (e / 51200.0 + 1e-12);
        };
        const auto infos = ZenProcessor::presetInfo();
        ZenProcessor cap;   // a quiet one (the main one still has notes held)
        cap.setPlayConfigDetails (0, 2, 48000.0, 512);
        cap.prepareToPlay (48000.0, 512);
        cap.setCurrentProgram (5);
        std::printf ("silence: %.1f dB\n", [&] { juce::MidiBuffer none; double e = 0; for (int b = 0; b < 500; ++b) { cap.processBlock (buf, none); } for (int b = 0; b < 100; ++b) { cap.processBlock (buf, none); for (int k = 0; k < 512; ++k) e += buf.getSample (0, k) * buf.getSample (0, k); } return 10.0 * std::log10 (e / 51200.0 + 1e-12); }());
        ZenProcessor::PatchRecipe fr; fr.kind = 1; fr.name = infos[12].name;
        cap.captureDrumPatch (7, fr);
        cap.captureDrumPatch (6, cap.snapshotOfCurrentSound());
        const double ms = waitCaptures (cap);
        std::printf ("captures: %.0f ms;  row 7 '%s' %.1f dB;  row 6 '%s' %.1f dB;  row 0 '%s' %.1f dB\n", ms,
                     cap.drumRowName (7).toRawUTF8(), rowEnergy (cap, 7), cap.drumRowName (6).toRawUTF8(), rowEnergy (cap, 6),
                     cap.drumRowName (0).toRawUTF8(), rowEnergy (cap, 0));
        juce::MemoryBlock st; cap.getStateInformation (st);
        ZenProcessor reopened;
        reopened.setPlayConfigDetails (0, 2, 48000.0, 512);
        reopened.prepareToPlay (48000.0, 512);
        reopened.setStateInformation (st.getData(), (int) st.getSize());
        juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
        waitCaptures (reopened);
        std::printf ("reopened session (%d bytes): row 7 '%s' %.1f dB;  row 6 '%s' %.1f dB\n", (int) st.getSize(),
                     reopened.drumRowName (7).toRawUTF8(), rowEnergy (reopened, 7), reopened.drumRowName (6).toRawUTF8(), rowEnergy (reopened, 6));
        cap.setDrumSound (6, (int) zen::Sound::kalimba);
        std::printf ("row 6 back to a library sound: '%s' %.1f dB\n", cap.drumRowName (6).toRawUTF8(), rowEnergy (cap, 6));
        }

    // app icon: the slab with a burned "z" and a brass ring
    {
        auto icon = wood::render (512);
        juce::Graphics g (icon);
        g.addTransform (juce::AffineTransform::scale (512.0f / 980.0f));
        const juce::Point<float> c (490.0f, 490.0f);
        juce::ColourGradient brass (theme::brassHi, c.x - 90.0f, c.y - 120.0f, theme::brassDk, c.x + 200.0f, c.y + 200.0f, true);
        brass.addColour (0.4, theme::brass); brass.addColour (0.75, theme::brassLo);
        g.setGradientFill (brass);
        g.drawEllipse (c.x - 250.0f, c.y - 250.0f, 500.0f, 500.0f, 34.0f);
        theme::burnText (g, "z", { c.x - 300.0f, c.y - 330.0f, 600.0f, 600.0f }, juce::Justification::centred,
                         theme::font (theme::FontKind::serif, 520.0f));
        juce::File f ("/tmp/zen_icon.png");
        f.deleteFile();
        juce::FileOutputStream os (f);
        juce::PNGImageFormat().writeImageToStream (icon, os);
    }

    {   // show a patch row on the BEAT page
        ZenProcessor::PatchRecipe fr; fr.kind = 1; fr.name = ZenProcessor::presetInfo()[3].name;
        proc.captureDrumPatch (7, fr);
        for (int i = 0; i < 50 && proc.capturesRunning.load() > 0; ++i) juce::MessageManager::getInstance()->runDispatchLoopUntil (20);
    }
    const double t0 = juce::Time::getMillisecondCounterHiRes();
    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
    ed->setSize (1600, 1000);
    std::printf ("editor + slab at 980 px: %.0f ms\n", juce::Time::getMillisecondCounterHiRes() - t0);
    auto* zen = dynamic_cast<ZenEditor*> (ed.get());

    if (std::getenv ("ZEN_MOCKUP"))
    {
        // a still mockup of the landscape layout (1600 x 1000 design units), built from the real widgets
        struct Mock : public juce::Component
        {
            juce::Image bg;
            std::vector<std::pair<juce::String, juce::Rectangle<float>>> labels;
            std::vector<std::tuple<juce::String, juce::Rectangle<float>, float>> serif;
            std::vector<std::tuple<juce::String, float, float>> arcs;   // text, radius, angle (deg) around the porthole
            bool curvedStrips = false;
            void paint (juce::Graphics& g) override
            {
                g.drawImage (bg, getLocalBounds().toFloat());
                for (auto& [t, r, sz] : serif) theme::burnText (g, t, r, juce::Justification::centred, theme::font (theme::FontKind::serif, sz));
                // the PITCH and MOD strips, straight down the two ends
                for (auto [x, dot, name] : { std::tuple<float, float, const char*> { 74.0f, 0.5f, "PITCH" }, { 1526.0f, 0.78f, "MOD" } })
                {
                    if (curvedStrips) break;
                    const juce::Rectangle<float> groove (x - 9.0f, 240.0f, 18.0f, 600.0f);
                    g.setColour (juce::Colour (0x59ffecc8)); g.fillRoundedRectangle (groove.translated (0, 1.0f).expanded (2.0f), 10.0f);
                    g.setColour (juce::Colour (0x8c3b1f0c)); g.fillRoundedRectangle (groove, 9.0f);
                    g.setColour (theme::burn.withAlpha (0.35f)); g.drawLine (x - 14.0f, 540.0f, x + 14.0f, 540.0f, 1.2f);
                    const float y = groove.getBottom() - dot * groove.getHeight();
                    juce::ColourGradient br (theme::brassHi, x - 6.0f, y - 8.0f, theme::brassDk, x + 10.0f, y + 10.0f, true);
                    br.addColour (0.4, theme::brass);
                    g.setGradientFill (br); g.fillEllipse (x - 13.0f, y - 13.0f, 26.0f, 26.0f);
                    theme::burnText (g, name, { x - 40.0f, 852.0f, 80.0f, 14.0f }, juce::Justification::centred, theme::font (theme::FontKind::sansBold, 11.5f, 0.2f));
                }
                for (auto& [t, r, a] : arcs)
                    theme::arcText (g, t, { 800.0f, 430.0f }, r, juce::degreesToRadians (a), theme::font (theme::FontKind::sansBold, 11.5f, 0.22f));
                for (auto& [t, r] : labels) theme::burnText (g, t, r, juce::Justification::centred, theme::font (theme::FontKind::sansBold, 11.5f, 0.2f));
            }
        };
        Mock m;
        m.setSize (1600, 1000);
        m.bg = plank::render (1600);
        SlabLookAndFeel lnf; m.setLookAndFeel (&lnf);
        std::vector<std::unique_ptr<juce::Component>> kids;
        auto place = [&] (juce::Component* c, juce::Point<float> pos, float w, float h, float rot = 0.0f, float anchorY = -1.0f)
        {
            const float ay = anchorY < 0.0f ? h * 0.5f : anchorY;
            c->setBounds (juce::roundToInt (pos.x - w * 0.5f), juce::roundToInt (pos.y - ay), juce::roundToInt (w), juce::roundToInt (h));
            if (rot != 0.0f) c->setTransform (juce::AffineTransform::rotation (juce::degreesToRadians (rot), pos.x, pos.y));
            m.addAndMakeVisible (c);
            kids.emplace_back (c);
        };
        auto steel = [&] (const char* t, juce::Point<float> pos, float dome = 22.0f, bool led = true, bool on = false)
        {
            auto* b = new SteelButton (t, dome); b->showLed = led; b->setToggleState (on, juce::dontSendNotification);
            if (on) b->setLedLevel (1.0f);
            place (b, pos, 64.0f, 56.0f, 0.0f, 23.0f);
        };
        const juce::Point<float> C (800.0f, 430.0f);
        auto pol = [&] (float r, float deg) { return theme::polar (C, r, deg); };

        // top: title, tabs, preset
        m.serif.push_back ({ "zenthesizer", { 150.0f, 88.0f, 260.0f, 44.0f }, 36.0f });
        const char* tabNames[] = { "SYNTH", "MOD", "WAVES", "BEAT", "FX", "PLAY", "STAGE" };
        for (int i = 0; i < 7; ++i)
        {
            auto* t = new TabButton (tabNames[i]); t->setToggleState (i == 0, juce::dontSendNotification);
            place (t, { 560.0f + 80.0f * (float) i, 112.0f }, 76.0f, 34.0f);
        }
        steel ("", { 1240.0f, 98.0f }, 18.0f, false);
        steel ("", { 1480.0f, 98.0f }, 18.0f, false);
        m.serif.push_back ({ "Temple Mist", { 1270.0f, 92.0f, 180.0f, 36.0f }, 26.0f });
        m.labels.push_back ({ "PREV", { 1200.0f, 124.0f, 80.0f, 14.0f } });
        m.labels.push_back ({ "NEXT", { 1440.0f, 124.0f, 80.0f, 14.0f } });

        // the synth core, as on today's slab
        for (int i = 0; i < 10; ++i)
        {
            const float a = -60.0f + 120.0f / 9.0f * (float) i;
            const int src = i < 9 ? i + 1 : (int) zen::Src::slide;
            auto* b = new SteelButton (ZenProcessor::srcShortNames()[src], 22.0f); b->ledColour = theme::srcColour (src);
            place (b, pol (255.0f, a), 52.0f, 54.0f, a, 23.0f);
        }
        const char* kn[] = { "CUTOFF", "RES", "DRIVE", "ENV", "ATTACK", "DECAY", "SUSTAIN", "RELEASE" };
        const float kv[] = { 0.62f, 0.15f, 0.1f, 0.62f, 0.8f, 0.7f, 0.85f, 0.85f };
        for (int i = 0; i < 8; ++i)
        {
            const float a = -66.0f + (float) i * 132.0f / 7.0f;
            auto* k = new WoodKnob (kn[i], 42.0f); k->setRange (0.0, 1.0); k->setValue (kv[i]);
            k->labelDir = { std::sin (juce::degreesToRadians (a)), -std::cos (juce::degreesToRadians (a)) };
            if (i == 0 || i == 7) k->labelDir = { 0.0f, 0.82f };
            place (k, pol (175.0f, a), 120.0f, 120.0f);
        }
        { auto* ph = new Porthole (proc.display()); place (ph, C, 218.0f, 218.0f); for (int f = 0; f < 40; ++f) ph->tick (0.03f, 2); }
        { auto* ea = new EngineBox (proc, false); place (ea, { 522.0f, 440.0f }, 185.0f, 200.0f); ea->tick (0.0f, false); }
        { auto* eb = new EngineBox (proc, true);  place (eb, { 1078.0f, 440.0f }, 185.0f, 200.0f); eb->tick (0.0f, false); }

        // timbre plates, centred above the keys
        for (int i = 0; i < 4; ++i)
        {
            auto* p = new BrassPlate (juce::String::charToString ((juce::juce_wchar) ('A' + i)), 56.0f);
            const char* sub[] = { "BLOOM", "MORPH", "ECHOES", "SPACE" };
            p->subLabel = sub[i];
            const bool fin2 = juce::String (std::getenv ("ZEN_MOCKKEYS")) == "final";
            place (p, { fin2 ? 620.0f + 120.0f * (float) i : 650.0f + 100.0f * (float) i, 618.0f }, 126.0f, 126.0f);
        }
        m.labels.push_back ({ "TIMBRE", { 740.0f, 690.0f, 120.0f, 14.0f } });

        // the keys, piano-style: black keys a row up, a slight smile across the row.
        // ZEN_MOCKKEYS=wide: an octave and a half (C3 to F4); big: one octave, larger plates
        const juce::String mode (std::getenv ("ZEN_MOCKKEYS"));
        const bool wide = mode == "wide" || mode == "final";
        const bool fin = mode == "final";   // an octave and a half with larger keys
        const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        const bool inScale[] = { true, false, false, true, false, true, false, true, false, false, true, false };   // C minor pentatonic
        const int numKeys = wide ? 18 : 12;
        const float spacing = fin ? 94.0f : (wide ? 84.0f : 116.0f), whiteDia = fin ? 58.0f : (wide ? 50.0f : 64.0f), blackDia = fin ? 50.0f : (wide ? 44.0f : 56.0f);
        int numWhites = 0; for (int k = 0; k < numKeys; ++k) if (! juce::String (names[k % 12]).contains ("#")) ++numWhites;
        const float mid = fin ? 800.0f : (wide ? 850.0f : 800.0f);
        const float x0 = mid - spacing * (float) (numWhites - 1) * 0.5f;
        const float bend = fin ? 0.00012f : 0.00018f;
        auto curve = [mid, bend] (float x) { return -bend * (x - mid) * (x - mid); };
        int w = 0;
        for (int k = 0; k < numKeys; ++k)
        {
            const int n = k % 12;
            const bool black = juce::String (names[n]).contains ("#");
            const float x = black ? x0 + spacing * ((float) w - 0.5f) : x0 + spacing * (float) w;
            const juce::String sub = juce::String (names[n]) + juce::String (3 + k / 12);
            auto* p = new BrassPlate (names[n], black ? blackDia : whiteDia); p->subLabel = sub;
            if (! inScale[n]) p->setAlpha (0.38f);
            if (k == 7) p->setLevel (0.9f);
            const float box = (black ? blackDia : whiteDia) + 66.0f;
            place (p, { x, (black ? (wide ? 752.0f : 742.0f) : (wide ? 852.0f : 858.0f)) + curve (x) }, box, box);
            if (! black) ++w;
        }
        const float xL = x0 - spacing * 0.5f - (wide ? 42.0f : 36.0f), xR = x0 + spacing * (float) (numWhites - 1) + spacing * 0.5f + (wide ? 42.0f : 36.0f);
        if (fin)
        {
            steel ("OCT -", { 1350.0f, 846.0f }, 18.0f, false);
            steel ("OCT +", { 1424.0f, 846.0f }, 18.0f, false);
            m.labels.push_back ({ "OCTAVE  C3", { 1327.0f, 802.0f, 120.0f, 14.0f } });
        }
        else
        {
            steel ("OCT -", { xL, 828.0f + curve (xL) }, 18.0f, false);
            steel ("OCT +", { xR, 828.0f + curve (xR) }, 18.0f, false);
            m.labels.push_back ({ "C3", { xR - 40.0f, 870.0f + curve (xR), 80.0f, 14.0f } });
        }

        if (fin)
        {
            // PITCH and MOD bow gently outward, like the rest of the sides
            m.curvedStrips = true;
            auto* ps = new ArcStrip ("PITCH", true, { 74.0f + 900.0f, 530.0f }, 900.0f, 250.5f, 289.5f);
            m.addAndMakeVisible (ps); kids.emplace_back (ps);
            auto* ms = new ArcStrip ("MOD", false, { 1526.0f - 900.0f, 530.0f }, 900.0f, 109.5f, 70.5f);
            ms->setShown (0.78f);
            m.addAndMakeVisible (ms); kids.emplace_back (ms);
        }

        // left: pitch strip, then the chords

        if (! fin) m.labels.push_back ({ "CHORDS  -  C MINOR", { 150.0f, 196.0f, 220.0f, 14.0f } });
        else m.arcs.push_back ({ "CHORDS  -  C MINOR", 624.0f, 270.0f });
        const juce::String rn[] = { "i", juce::String (juce::CharPointer_UTF8 ("ii\xc2\xb0")), "III", "iv", "v", "VI", "VII" };
        const char* cn[] = { "Cm", "Ddim", "Eb", "Fm", "Gm", "Ab", "Bb" };
        for (int i = 0; i < 7; ++i)
        {
            auto* p = new BrassPlate (rn[i], 48.0f); p->subLabel = cn[i];
            if (i == 3) p->setLevel (0.9f);
            if (fin)
            {
                // a curved column around the porthole, top to bottom
                place (p, pol (568.0f, 297.0f - 9.0f * (float) i), 110.0f, 110.0f);
                continue;
            }
            const float x = i < 6 ? (i % 2 == 0 ? 205.0f : 315.0f) : 260.0f;
            const float y = 262.0f + 98.0f * (float) (i / 2);
            place (p, { x, y }, 120.0f, 120.0f);
        }
        if (fin)
        {
            // the chord modifiers on an inner arc beside them
            const char* mods[] = { "7TH", "9TH", "SUS", "SPREAD", "STRUM" };
            for (int i = 0; i < 5; ++i)
            {
                auto* b = new SteelButton (mods[i], 20.0f); b->setToggleState (i == 0 || i == 3, juce::dontSendNotification);
                if (i == 0 || i == 3) b->setLedLevel (1.0f);
                place (b, pol (448.0f, 290.0f - 10.0f * (float) i), 64.0f, 56.0f, 0.0f, 23.0f);
            }
            { auto* k = new WoodKnob ("SPEED", 32.0f); k->setRange (0.0, 1.0); k->setValue (0.4); k->labelDir = { 0.0f, 0.82f }; place (k, pol (448.0f, 238.0f), 84.0f, 84.0f); }
        }
        else
        {
            steel ("7TH", { 180.0f, 646.0f }, 20.0f, true, true);
            steel ("9TH", { 260.0f, 646.0f }, 20.0f);
            steel ("SUS", { 340.0f, 646.0f }, 20.0f);
            steel ("SPREAD", { 220.0f, 726.0f }, 20.0f, true, true);
            steel ("STRUM", { 300.0f, 726.0f }, 20.0f);
            { auto* k = new WoodKnob ("STRUM SPEED", 34.0f); k->setRange (0.0, 1.0); k->setValue (0.4); k->labelDir = { 0.0f, 0.82f }; place (k, { 260.0f, 826.0f }, 100.0f, 100.0f); }
        }

        // right: looper, visuals, mod strip
        if (fin)
        {
            // the mirror of the left: looper and visuals on arcs around the porthole
            m.arcs.push_back ({ "LOOPER", 624.0f, 72.0f });
            m.arcs.push_back ({ "VISUALS", 624.0f, 108.0f });
            steel ("LOOP", pol (568.0f, 63.0f), 22.0f, true, true);
            steel ("STOP", pol (568.0f, 72.0f), 22.0f);
            steel ("UNDO", pol (568.0f, 81.0f), 22.0f);
            { auto* k = new WoodKnob ("LOOP LVL", 40.0f); k->setRange (0.0, 1.0); k->setValue (0.8); k->labelDir = { 0.0f, 0.82f }; place (k, pol (448.0f, 74.0f), 100.0f, 100.0f); }
            steel ("OFF", pol (568.0f, 99.0f), 20.0f);
            steel ("LOW", pol (568.0f, 108.0f), 20.0f);
            steel ("FULL", pol (568.0f, 117.0f), 20.0f, true, true);
            { auto* b = new SteelButton ("FULL SCREEN", 18.0f); b->showLed = false; place (b, pol (448.0f, 112.0f), 110.0f, 56.0f, 0.0f, 23.0f); }
        }
        else
        {
        m.labels.push_back ({ "LOOPER", { 1300.0f, 220.0f, 140.0f, 14.0f } });
        steel ("LOOP", { 1290.0f, 266.0f }, 22.0f, true, true);
        steel ("STOP", { 1370.0f, 266.0f }, 22.0f);
        steel ("UNDO", { 1450.0f, 266.0f }, 22.0f);
        { auto* k = new WoodKnob ("LOOP LVL", 40.0f); k->setRange (0.0, 1.0); k->setValue (0.8); k->labelDir = { 0.0f, 0.82f }; place (k, { 1370.0f, 400.0f }, 110.0f, 110.0f); }
        m.labels.push_back ({ "VISUALS", { 1300.0f, 520.0f, 140.0f, 14.0f } });
        steel ("OFF", { 1290.0f, 566.0f }, 20.0f);
        steel ("LOW", { 1370.0f, 566.0f }, 20.0f);
        steel ("FULL", { 1450.0f, 566.0f }, 20.0f, true, true);
        }

        if (! fin) { auto* b = new SteelButton ("FULL SCREEN", 18.0f); b->showLed = false; place (b, { 1370.0f, 740.0f }, 110.0f, 56.0f, 0.0f, 23.0f); }

        juce::MessageManager::getInstance()->runDispatchLoopUntil (200);
        auto img = m.createComponentSnapshot (m.getLocalBounds(), true, 1.0f);
        juce::File f (std::getenv ("ZEN_MOCKUP")); f.deleteFile();
        juce::FileOutputStream os (f); juce::PNGImageFormat().writeImageToStream (img, os);
        std::printf ("wrote %s\n", f.getFullPathName().toRawUTF8());
        m.setLookAndFeel (nullptr);
        return 0;
    }

    if (std::getenv ("ZEN_FULLTEST"))
    {
        ed->addToDesktop (0); ed->setVisible (true);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (300);
        zen->setFullscreen (true);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (800);
        for (int d = 0; d < juce::Desktop::getInstance().getNumComponents(); ++d)
        {
            auto* c = juce::Desktop::getInstance().getComponent (d);
            std::printf ("window %d: %s %s\n", d, typeid (*c).name(), c->getScreenBounds().toString().toRawUTF8());
            if (c != ed.get())
            {
                auto img = c->createComponentSnapshot (c->getLocalBounds(), true, 0.5f);
                juce::File f ("/tmp/zen_full.png"); f.deleteFile(); juce::FileOutputStream os (f); juce::PNGImageFormat().writeImageToStream (img, os);
            }
        }
        { auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 0.5f);
          juce::File f ("/tmp/zen_full_editor.png"); f.deleteFile(); juce::FileOutputStream os (f); juce::PNGImageFormat().writeImageToStream (img, os); }
        zen->setFullscreen (false);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (300);
        std::printf ("after leaving: windows %d, content parent is editor: %d\n", juce::Desktop::getInstance().getNumComponents(),
                     zen->getContent().getParentComponent() == ed.get());
        { auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 0.5f);
          juce::File f ("/tmp/zen_full_back.png"); f.deleteFile(); juce::FileOutputStream os (f); juce::PNGImageFormat().writeImageToStream (img, os); }
        return 0;
    }

    if (std::getenv ("ZEN_MENUTEST"))
    {
        // click the preset name, then click an item in the menu, through the real window system
        ed->setTopLeftPosition (0, 0);
        ed->addToDesktop (0);
        ed->setVisible (true);
        auto pump = [] (int ms) { juce::MessageManager::getInstance()->runDispatchLoopUntil (ms); };
        pump (300);
        auto* peer = ed->getPeer();
        // real pointer events through the XTest extension (JUCE menus read the real pointer)
        void* x11 = dlopen ("libX11.so.6", RTLD_NOW); void* xtst = dlopen ("libXtst.so.6", RTLD_NOW);
        using OpenFn = void* (*) (const char*); using MotionFn = int (*) (void*, int, int, int, unsigned long);
        using ButtonFn = int (*) (void*, unsigned, int, unsigned long); using FlushFn = int (*) (void*);
        auto xOpen = (OpenFn) dlsym (x11, "XOpenDisplay"); auto xFlush = (FlushFn) dlsym (x11, "XFlush");
        auto fakeMotion = (MotionFn) dlsym (xtst, "XTestFakeMotionEvent"); auto fakeButton = (ButtonFn) dlsym (xtst, "XTestFakeButtonEvent");
        void* xd = xOpen (nullptr);
        auto moveTo = [&] (juce::Component* comp, juce::Point<float> pos)
        {
            const auto sp = comp->localPointToGlobal (pos).roundToInt();
            fakeMotion (xd, -1, sp.x, sp.y, 0); xFlush (xd); pump (150);
            fakeMotion (xd, -1, sp.x + 1, sp.y, 0); xFlush (xd); pump (150);
        };
        auto clickAt = [&] (juce::Component* comp, juce::Point<float> pos)
        {
            moveTo (comp, pos);
            fakeButton (xd, 1, 1, 0); xFlush (xd); pump (80);
            fakeButton (xd, 1, 0, 0); xFlush (xd); pump (400);
        };
        auto click = [&] (juce::ComponentPeer* pr, juce::Point<float> pos) { clickAt (&pr->getComponent(), pos); };
        auto& content = zen->getContent();
        // preset name sits at 490, 858 in design units
        const auto namePos = ed->getLocalPoint (&content, juce::Point<float> (1360.0f, 108.0f));
        std::printf ("before: %s\n", proc.currentPresetName().toRawUTF8());
        click (peer, namePos);
        // menus now open inside the editor: find the menu windows among its children
        auto menus = [&]
        {
            std::vector<juce::Component*> found;
            std::function<void (juce::Component&)> walk = [&] (juce::Component& c)
            {
                for (auto* ch : c.getChildren())
                {
                    if (juce::String (typeid (*ch).name()).contains ("MenuWindow") && ch->isVisible()) found.push_back (ch);
                    walk (*ch);
                }
            };
            walk (*ed);
            for (int d = 0; d < juce::Desktop::getInstance().getNumComponents(); ++d)
                if (juce::String (typeid (*juce::Desktop::getInstance().getComponent (d)).name()).contains ("MenuWindow")) found.push_back (juce::Desktop::getInstance().getComponent (d));
            return found;
        };
        auto items = [] (juce::Component* w)
        {
            std::vector<juce::Component*> v;
            for (auto* ch : w->getChildren()) if (juce::String (typeid (*ch).name()).contains ("ItemComponent")) v.push_back (ch);
            return v;
        };
        auto ms = menus();
        std::printf ("menus open after clicking the name: %d (inside the editor: %s)\n", (int) ms.size(),
                     ms.empty() ? "-" : (ms[0]->getParentComponent() != nullptr ? "yes" : "no"));
        if (! ms.empty())
        {
            auto top = items (ms[0]);
            std::printf ("top-level items: %d\n", (int) top.size());
            // hover the second folder to open it, then click the third preset inside it
            if (std::getenv ("ZEN_MENUSHOT") && top.size() > 8)
            {
                moveTo (top[8], top[8]->getLocalBounds().getCentre().toFloat()); pump (700);
                auto ms2 = menus();
                if (ms2.size() > 1) { auto sub = items (ms2.back()); if (! sub.empty()) { moveTo (sub[0], sub[0]->getLocalBounds().getCentre().toFloat()); pump (700); } }
                auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
                juce::File f ("/tmp/zen_menu.png"); f.deleteFile(); juce::FileOutputStream os (f); juce::PNGImageFormat().writeImageToStream (img, os);
                return 0;
            }
            juce::Component* folder = top.size() > 2 ? top[2] : nullptr;
            if (folder != nullptr)
            {
                moveTo (folder, folder->getLocalBounds().getCentre().toFloat());
                pump (600);
                ms = menus();
                std::printf ("menus open after hovering a folder: %d\n", (int) ms.size());
                if (ms.size() > 1)
                {
                    auto subItems = items (ms.back());
                    std::printf ("items in that folder: %d\n", (int) subItems.size());
                    if (subItems.size() > 2)
                    {
                        moveTo (subItems[2], subItems[2]->getLocalBounds().getCentre().toFloat());
                        clickAt (subItems[2], subItems[2]->getLocalBounds().getCentre().toFloat());
                        pump (500);
                    }
                }
            }
        }
        std::printf ("after: %s  (desktop components now %d)\n", proc.currentPresetName().toRawUTF8(), juce::Desktop::getInstance().getNumComponents());
        if (std::getenv ("ZEN_MENUCONTROL"))
        {
            juce::PopupMenu::dismissAllActiveMenus(); pump (200);
            juce::PopupMenu m; for (int i = 1; i <= 10; ++i) m.addItem (i, "item " + juce::String (i));
            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (ed.get()), [] (int r) { std::printf ("control menu result %d\n", r); });
            pump (300);
            auto* w = juce::Desktop::getInstance().getComponent (juce::Desktop::getInstance().getNumComponents() - 1);
            juce::Component* it = nullptr;
            for (auto* ch : w->getChildren()) if (juce::String (typeid (*ch).name()).contains ("ItemComponent") && ch->getY() > 40) { it = ch; break; }
            if (it) { const auto p = w->getLocalPoint (it, it->getLocalBounds().getCentre().toFloat());
                      pump (400); click (w->getPeer(), p); pump (400); }
        }
        return 0;
    }

    const char* names[] = { "synth", "mod", "waves", "beat", "fx", "play", "stage" };
    { auto d = proc.getDrums(); d.playing = true; proc.setDrums (d); }
    for (int p = 0; p < 7; ++p)
    {
        zen->getContent().showPage (p);
        for (int k = 0; k < 6; ++k)
        {
            juce::MidiBuffer midi;
            proc.processBlock (buf, midi);
            juce::MessageManager::getInstance()->runDispatchLoopUntil (60);
        }
        if (p == 2)   // also show the harmonics view
        {
            std::function<juce::Component* (juce::Component&)> find = [&] (juce::Component& c) -> juce::Component*
            {
                if (c.getComponentID() == "harmTab") return &c;
                for (auto* ch : c.getChildren()) if (auto* f = find (*ch)) return f;
                return nullptr;
            };
            if (auto* tab = dynamic_cast<juce::Button*> (find (zen->getContent()))) tab->triggerClick();
        }
        juce::MessageManager::getInstance()->runDispatchLoopUntil (80);
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        juce::File f ("/tmp/zen_" + juce::String (names[p]) + ".png");
        f.deleteFile();
        juce::FileOutputStream os (f);
        juce::PNGImageFormat().writeImageToStream (img, os);
        std::printf ("wrote %s\n", f.getFullPathName().toRawUTF8());
    }
    return 0;
}
