#include <typeinfo>
#include <dlfcn.h>
// Developer tool: plays a few notes, then renders each page of the editor to /tmp/zen_*.png.
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets.h"

int main()
{
    std::setvbuf (stdout, nullptr, _IONBF, 0);
    juce::ScopedJuceInitialiser_GUI init;
    ZenProcessor proc;
    proc.setPlayConfigDetails (0, 2, 48000.0, 512);
    proc.prepareToPlay (48000.0, 512);

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
    ed->setSize (980, 980);
    std::printf ("editor + slab at 980 px: %.0f ms\n", juce::Time::getMillisecondCounterHiRes() - t0);
    auto* zen = dynamic_cast<ZenEditor*> (ed.get());

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
        const auto namePos = ed->getLocalPoint (&content, juce::Point<float> (490.0f, 858.0f));
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
