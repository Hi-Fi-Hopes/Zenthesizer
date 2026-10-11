// Developer tool: plays a few notes, then renders each page of the editor to /tmp/zen_*.png.
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets.h"

int main()
{
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

    const double t0 = juce::Time::getMillisecondCounterHiRes();
    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
    ed->setSize (980, 980);
    std::printf ("editor + slab at 980 px: %.0f ms\n", juce::Time::getMillisecondCounterHiRes() - t0);
    auto* zen = dynamic_cast<ZenEditor*> (ed.get());

    const char* names[] = { "synth", "mod", "waves", "fx", "play", "stage" };
    for (int p = 0; p < 6; ++p)
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
