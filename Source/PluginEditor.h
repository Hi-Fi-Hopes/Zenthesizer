#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "ui/Widgets.h"
#include "ui/Wood.h"
#include "ui/WavePanel.h"
#include "ui/BeatPanel.h"
#include <map>
#include <set>

//==============================================================================
// Engine A or B: a carved outline, a little glass screen, four selector buttons and four knobs.
class EngineBox : public juce::Component
{
public:
    EngineBox (ZenProcessor&, bool isB);
    void paint (juce::Graphics&) override;
    void resized() override;
    void tick (float dt, bool animate);
    void mouseDown (const juce::MouseEvent&) override;

    std::vector<std::unique_ptr<SteelButton>> selectors;
    std::function<void()> onScreenClick;
    std::array<std::unique_ptr<WoodKnob>, 4> knobs;

private:
    ZenProcessor& proc;
    bool isB;
    float phase = 0.0f;
    juce::Rectangle<float> screen;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> attachments;
    void paintScreenA (juce::Graphics&, juce::Rectangle<float>);
    void paintScreenB (juce::Graphics&, juce::Rectangle<float>);
};

//==============================================================================
// One row of the mod matrix (MOD page): source -> target, amount, clear.
class SlotRow : public juce::Component
{
public:
    SlotRow (ZenProcessor&, int index);
    void resized() override;
    void paint (juce::Graphics&) override;
    void refreshColour();

private:
    ZenProcessor& proc;
    int index;
    juce::ComboBox src, dst;
    AmountSlider amount;
    juce::TextButton clear { "x" };
    juce::AudioProcessorValueTreeState::ComboBoxAttachment srcAttach, dstAttach;
    juce::AudioProcessorValueTreeState::SliderAttachment amtAttach;
};

//==============================================================================
// Everything on the slab, at the design size (980 x 980). The editor scales it.
class Content : public juce::Component, private juce::Timer
{
public:
    explicit Content (ZenProcessor&);
    ~Content() override;
    void paint (juce::Graphics&) override;
    void resized() override {}
    void mouseDown (const juce::MouseEvent&) override;
    void setRenderScale (float pixelsPerUnit);

    enum Page { synth = 0, mod, waves, beat, fx, play, stage, numPages };
    void showPage (int);
    void showHint (const juce::String&);

private:
    void timerCallback() override;
    void setupSynthPage();
    void setupKnob (WoodKnob&, const char* paramId, int dst);
    void place (juce::Component&, juce::Point<float> centre, float w, float h, float rotationDeg = 0.0f, float anchorY = -1.0f);
    void arm (int src);
    void updateModRings();
    void showPresetMenu();
    void savePresetAs();
    void stepPreset (int delta);
    juce::String dstShort (int dst) const;
    void setParam (const juce::String& id, float plainValue);
    float param (const char* id) const { return proc.apvts.getRawParameterValue (id)->load(); }

    ZenProcessor& proc;
    SlabLookAndFeel lnf;
    juce::Image slab;
    float slabPixels = 0.0f;
    std::atomic<bool> rendering { false };
    int page = synth;
    int armedSrc = 0;
    double lastTick = 0.0;
    int learnPlate = -1, lastInCount = 0;
    bool learning = false;

    // always there
    std::array<std::unique_ptr<TabButton>, numPages> tabs;
    std::array<std::unique_ptr<SteelButton>, 3> visButtons;
    std::array<std::unique_ptr<BrassPlate>, zen::numPlates> plates;
    SteelButton prevBtn { "PREV", 16.0f }, nextBtn { "NEXT", 16.0f };
    SteelButton loopBtn { "LOOP", 22.0f }, loopStopBtn { "STOP", 22.0f }, loopUndoBtn { "UNDO", 22.0f };
    WoodKnob loopLevelKnob { "LOOP LVL", 32.0f };
    ArcStrip pitchStrip { "PITCH", true,  { 490.0f, 490.0f }, 395.0f, 254.0f, 286.0f };
    ArcStrip modStrip   { "MOD",   false, { 490.0f, 490.0f }, 395.0f, 106.0f, 74.0f };
    juce::ComboBox bendBox;
    juce::Component presetHit;

    // SYNTH page
    Porthole porthole;
    std::array<std::unique_ptr<WoodKnob>, 8> mainKnobs;
    std::array<std::unique_ptr<SteelButton>, 9> modButtons;
    EngineBox engineA, engineB;

    // MOD page
    std::vector<std::unique_ptr<WoodKnob>> modPageKnobs;
    juce::ComboBox lfo1Shape, lfo2Shape;
    std::array<std::unique_ptr<SlotRow>, zen::numSlots> slotRows;
    std::array<std::unique_ptr<TabButton>, 2> slotPageBtns;
    int slotPage = 0;

    // FX page
    std::vector<std::unique_ptr<WoodKnob>> fxKnobs;
    juce::ComboBox delaySync;

    // PLAY page
    juce::ComboBox rootBox, scaleBox, octaveBox, arpModeBox, arpRateBox, arpOctBox, padChanBox;
    SteelButton latchBtn { "LATCH A-D" }, arpOnBtn { "ARP" }, arpHoldBtn { "HOLD" }, padsBtn { "PADS" },
                learnBtn { "LEARN" }, resetPadsBtn { "RESET" };
    WoodKnob arpGate { "GATE" };

    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliderAttachments;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>> comboAttachments;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> buttonAttachments;

    struct PageLabel { int page; juce::String text; juce::Point<float> pos; };
    std::vector<PageLabel> pageLabels;
    std::vector<juce::Component*> pageComps[numPages];
    juce::String hint;
    juce::uint32 hintTime = 0;
    WavePanel wavePanel;
    BeatPanel beatPanel;
    int renderGeneration = 0;
    void finishSlab (juce::Image, int pixels);
};

//==============================================================================
class ZenEditor : public juce::AudioProcessorEditor
{
public:
    explicit ZenEditor (ZenProcessor&);
    void resized() override;
    void paint (juce::Graphics& g) override { g.fillAll (theme::table); }

    static constexpr int designSize = 980;
    Content& getContent() { return content; }

    ~ZenEditor() override;

private:
    Content content;
    juce::ComponentBoundsConstrainer constrainer;
    juce::TooltipWindow tooltips { this, 700 };

    // zoom: the slab is drawn at baseScale * zoom, with its top-left corner at origin
    float baseScale = 1.0f, zoom = 1.0f;
    juce::Point<float> origin;
    void applyZoom (bool sharpen);
    void clampOrigin();
    void zoomAround (juce::Point<float> editorPos, float newZoom, juce::Point<float> anchorContent);

   #if JUCE_ANDROID || JUCE_IOS
    SteelButton midiBtn { "BLUETOOTH", 26.0f }, audioBtn { "AUDIO", 26.0f }, fitBtn { "FIT", 26.0f };

    // Pinch, drag and double-tap on bare wood (listens to every touch on screen).
    struct Gestures : public juce::MouseListener
    {
        explicit Gestures (ZenEditor& e) : ed (e) {}
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;

        ZenEditor& ed;
        std::map<int, juce::Point<float>> touches;
        std::set<int> wood;
        bool pinching = false, pinchPending = false, moved = false;
        std::map<int, juce::uint32> downTime;
        std::map<int, juce::Point<float>> downPos;
        int pinchA = -1, pinchB = -1;
        float startDist = 1.0f, startZoom = 1.0f;
        juce::Point<float> anchor, panStartPos, panStartOrigin;
        juce::uint32 lastTap = 0;
        juce::Point<float> lastTapPos;
        void beginPan (int index);
    };
    Gestures gestures { *this };
   #endif
};
