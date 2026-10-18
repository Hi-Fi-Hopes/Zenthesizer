#pragma once

// The controls on the slab: walnut knobs, steel buttons with indicator lights, brass plates,
// burned-text tabs and menus, the porthole, and the mod-slot rows.

#include "Theme.h"
#include "../dsp/Engine.h"
#include "../dsp/Voice.h"
#include "../dsp/Wavetables.h"
#include <functional>
#include <vector>

//==============================================================================
// Touch zoom: while two fingers are pinching the bare wood, any other finger is ignored
// by knobs and plates.
struct WoodTouch
{
    static inline int fingersOnWood = 0;   // > 0 only while a pinch is active
    static bool claims (const juce::MouseEvent& e) { return e.source.isTouch() && fingersOnWood > 0; }
};

//==============================================================================
class SlabLookAndFeel : public juce::LookAndFeel_V4
{
public:
    SlabLookAndFeel()
    {
        setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff24170c));
        setColour (juce::PopupMenu::textColourId, theme::bone);
        setColour (juce::PopupMenu::headerTextColourId, theme::amber);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (0xff6b4220));
        setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
        setColour (juce::ComboBox::textColourId, theme::burn);
        setColour (juce::ComboBox::backgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
        setColour (juce::ComboBox::arrowColourId, theme::burn);
        setColour (juce::Label::textColourId, theme::burn);
        setColour (juce::TooltipWindow::backgroundColourId, juce::Colour (0xff24170c));
        setColour (juce::TooltipWindow::textColourId, theme::bone);
    }

    juce::Font getPopupMenuFont() override               { return theme::font (theme::FontKind::sans, 15.0f); }

    // Menus open inside the plugin window, at their natural size. Separate OS-level menu windows
    // (scaled to match the slab) misbehave in some hosts and on touch screens.
    juce::Component* getParentComponentForMenuOptions (const juce::PopupMenu::Options& o) override
    {
        if (auto* p = o.getParentComponent()) return p;
        if (auto* t = o.getTargetComponent()) return t->getTopLevelComponent();
        return nullptr;
    }
    bool shouldPopupMenuScaleWithTargetComponent (const juce::PopupMenu::Options&) override { return false; }
    void getIdealPopupMenuItemSize (const juce::String& text, bool isSeparator, int standardHeight, int& w, int& h) override
    {
        LookAndFeel_V4::getIdealPopupMenuItemSize (text, isSeparator, standardHeight, w, h);
       #if JUCE_ANDROID || JUCE_IOS
        if (! isSeparator) h = std::max (h, 40);   // finger-sized rows
       #endif
    }
    juce::Font getComboBoxFont (juce::ComboBox&) override { return theme::font (theme::FontKind::sansBold, 12.5f, 0.08f); }
    juce::Font getLabelFont (juce::Label&) override       { return theme::font (theme::FontKind::sansBold, 12.5f, 0.08f); }

    void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box) override
    {
        const bool hover = box.isMouseOver (true);
        g.setColour (theme::burn.withAlpha (hover ? 0.75f : 0.45f));
        g.fillRect (2.0f, (float) h - 3.0f, (float) w - 4.0f, 1.5f);
        juce::Path caret;
        const float cx = (float) w - 9.0f, cy = (float) h * 0.5f;
        caret.addTriangle (cx - 4.0f, cy - 2.0f, cx + 4.0f, cy - 2.0f, cx, cy + 3.0f);
        g.fillPath (caret);
    }

    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (2, 0, box.getWidth() - 18, box.getHeight() - 2);
        label.setFont (getComboBoxFont (box));
        label.setJustificationType (juce::Justification::centred);
    }
};

// Shows a menu with the slab's look, inside the window that holds `target`.
inline void showMenu (juce::PopupMenu& m, juce::Component& target, std::function<void (int)> callback, int columns = 1)
{
    m.setLookAndFeel (&target.getLookAndFeel());
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&target)
                                               .withParentComponent (target.getTopLevelComponent())
                                               .withMaximumNumColumns (columns),
                     std::move (callback));
}

//==============================================================================
struct ModArc { juce::Colour colour; float amount; };

// Turned walnut knob with a brass dot. Coloured rings show what modulates it.
class WoodKnob : public juce::Slider
{
public:
    explicit WoodKnob (const juce::String& labelText = {}, float diameter = 42.0f)
        : label (labelText), dia (diameter)
    {
        setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        setRotaryParameters (juce::degreesToRadians (-135.0f), juce::degreesToRadians (135.0f), true);
        setMouseDragSensitivity (240);
        setVelocityBasedMode (false);
        setPaintingIsUnclipped (true);
    }

    juce::String label;
    float dia;
    int dst = 0;                                   // zen::Dst this knob drives (0 = none)
    std::function<bool (const juce::MouseEvent&, int)> modDrag;   // phase: 0 down, 1 drag, 2 up, 3 double-click
    juce::Colour armedColour = juce::Colours::transparentBlack;
    juce::Point<float> labelDir;   // non-zero: knob sits in the middle, label pushed out this way

    void setArcs (std::vector<ModArc> a)
    {
        bool same = a.size() == arcs.size();
        for (size_t i = 0; same && i < a.size(); ++i) same = a[i].colour == arcs[i].colour && std::abs (a[i].amount - arcs[i].amount) < 1.0e-4f;
        if (! same) { arcs = std::move (a); repaint(); }
    }
    void setLive (float v)
    {
        if (std::abs (v - live) > 0.002f) { live = v; if (! arcs.empty()) repaint(); }
    }

    juce::Point<float> knobCentre() const
    {
        if (labelDir != juce::Point<float>()) return { (float) getWidth() * 0.5f, (float) getHeight() * 0.5f };
        return { (float) getWidth() * 0.5f, 6.0f + dia * 0.5f };
    }

    void paint (juce::Graphics& g) override
    {
        const auto c = knobCentre();
        const float r = dia * 0.5f;
        const float a0 = juce::degreesToRadians (-135.0f), span = juce::degreesToRadians (270.0f);
        const float base = (float) valueToProportionOfLength (getValue());

        // the hole it is set into
        g.setColour (theme::burn.withAlpha (0.38f));
        g.fillEllipse (c.x - r - 3.0f, c.y - r - 3.0f, (r + 3.0f) * 2.0f, (r + 3.0f) * 2.0f);

        // armed for quick-assign: a coloured halo
        if (! armedColour.isTransparent())
        {
            g.setColour (armedColour.withAlpha (0.35f));
            g.drawEllipse (c.x - r - 7.0f, c.y - r - 7.0f, (r + 7.0f) * 2.0f, (r + 7.0f) * 2.0f, 2.0f);
        }

        // mod rings
        const float ringR = r + 6.0f;
        float ringOffset = 0.0f;
        for (auto& a : arcs)
        {
            const float from = juce::jlimit (0.0f, 1.0f, base), to = juce::jlimit (0.0f, 1.0f, base + a.amount);
            juce::Path p;
            p.addCentredArc (c.x, c.y, ringR + ringOffset, ringR + ringOffset, 0.0f, a0 + span * std::min (from, to), a0 + span * std::max (from, to), true);
            g.setColour (a.colour.withAlpha (0.9f));
            g.strokePath (p, juce::PathStrokeType (2.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            ringOffset += 3.0f;
        }
        if (! arcs.empty() && live >= 0.0f)
        {
            const float ang = a0 + span * juce::jlimit (0.0f, 1.0f, live);
            const juce::Point<float> p (c.x + ringR * std::sin (ang), c.y - ringR * std::cos (ang));
            g.setColour (theme::bone.withAlpha (0.35f));
            g.fillEllipse (p.x - 4.5f, p.y - 4.5f, 9.0f, 9.0f);
            g.setColour (theme::bone);
            g.fillEllipse (p.x - 2.2f, p.y - 2.2f, 4.4f, 4.4f);
        }

        // turned walnut body
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillEllipse (c.x - r, c.y - r + 3.0f, r * 2.0f, r * 2.0f);
        juce::ColourGradient body (theme::walnutHi, c.x - r * 0.3f, c.y - r * 0.4f, theme::walnutLo, c.x + r, c.y + r, true);
        body.addColour (0.55, theme::walnut);
        g.setGradientFill (body);
        g.fillEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f);
        // turning rings on the face
        g.setColour (juce::Colour (0x22ffe6be));
        for (float k = 0.35f; k < 0.95f; k += 0.18f) g.drawEllipse (c.x - r * k, c.y - r * k, r * 2.0f * k, r * 2.0f * k, 0.8f);
        g.setColour (juce::Colour (0x40ffe6be));
        g.drawEllipse (c.x - r + 0.5f, c.y - r + 0.5f, r * 2.0f - 1.0f, r * 2.0f - 1.0f, 1.0f);

        // brass pointer dot
        const float ang = a0 + span * base;
        const juce::Point<float> d (c.x + (r - 6.0f) * std::sin (ang), c.y - (r - 6.0f) * std::cos (ang));
        g.setColour (theme::brass.withAlpha (0.35f));
        g.fillEllipse (d.x - 4.0f, d.y - 4.0f, 8.0f, 8.0f);
        g.setColour (juce::Colour (0xfff0cf7a));
        g.fillEllipse (d.x - 2.6f, d.y - 2.6f, 5.2f, 5.2f);

        // label, or the value while touching it
        const bool showValue = isMouseOverOrDragging() || isMouseButtonDown();
        const juce::String text = showValue ? getTextFromValue (getValue()) : label;
        juce::Rectangle<float> labelBox (0.0f, c.y + r + 5.0f, (float) getWidth(), 14.0f);
        if (labelDir != juce::Point<float>())
        {
            const auto lc = c + labelDir * (r + 15.0f + std::abs (labelDir.x) * 10.0f);
            labelBox = juce::Rectangle<float> (80.0f, 14.0f).withCentre (lc);
        }
        theme::burnText (g, text, labelBox, juce::Justification::centred,
                         theme::font (showValue ? theme::FontKind::sansBold : theme::FontKind::sans, 11.0f, 0.1f));
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (WoodTouch::claims (e)) { blocked = true; return; }
        if (! (modDrag && modDrag (e, 0))) juce::Slider::mouseDown (e); else modding = true;
    }
    void mouseDrag (const juce::MouseEvent& e) override        { if (blocked) return; if (modding) modDrag (e, 1); else juce::Slider::mouseDrag (e); }
    void mouseUp (const juce::MouseEvent& e) override
    {
        if (blocked) { blocked = false; return; }
        if (modding) { modDrag (e, 2); modding = false; } else juce::Slider::mouseUp (e);
        repaint();
    }
    void mouseDoubleClick (const juce::MouseEvent& e) override { if (! (modDrag && modDrag (e, 3))) juce::Slider::mouseDoubleClick (e); }
    void mouseEnter (const juce::MouseEvent& e) override       { juce::Slider::mouseEnter (e); repaint(); }
    void mouseExit (const juce::MouseEvent& e) override        { juce::Slider::mouseExit (e); repaint(); }

    bool hitTest (int x, int y) override
    {
        const auto c = knobCentre();
        if (labelDir != juce::Point<float>()) return c.getDistanceFrom ({ (float) x, (float) y }) < dia * 0.5f + 8.0f;
        return c.getDistanceFrom ({ (float) x, (float) y }) < dia * 0.5f + 8.0f || y > c.y + dia * 0.5f;
    }

private:
    std::vector<ModArc> arcs;
    float live = -1.0f;
    bool modding = false, blocked = false;
};

//==============================================================================
// Steel push button set into the wood, with an indicator light above and a burned label below.
class SteelButton : public juce::Button, private juce::Timer
{
public:
    explicit SteelButton (const juce::String& labelText, float domeSize = 22.0f) : juce::Button (labelText), dome (domeSize)
    {
        setPaintingIsUnclipped (true);
    }

    // optional: press and hold
    std::function<void()> onHold;
    bool consumeHold() { const bool h = held; held = false; return h; }

    // optional: a progress ring around the dome (0..1, < 0 = none)
    float ring = -1.0f;
    juce::Colour ringColour = theme::amber;
    void setRing (float r, juce::Colour c)
    {
        if (std::abs (r - ring) > 0.003f || c != ringColour) { ring = r; ringColour = c; repaint(); }
    }

    void mouseDown (const juce::MouseEvent& e) override { held = false; if (onHold) startTimer (700); juce::Button::mouseDown (e); }
    void mouseUp (const juce::MouseEvent& e) override   { stopTimer(); juce::Button::mouseUp (e); }

    float dome;
    juce::Colour ledColour = theme::amber;
    bool showLed = true;
    float ledLevel = -1.0f;        // >= 0: light follows this (0..1) instead of the toggle state
    bool labelAbove = false;

    void setLedLevel (float v) { if (std::abs (v - ledLevel) > 0.01f) { ledLevel = v; repaint(); } }

    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        const float w = (float) getWidth();
        const float ledY = labelAbove ? 16.0f : 3.0f;
        const float domeY = ledY + (showLed ? 9.0f : 0.0f);
        const juce::Point<float> c (w * 0.5f, domeY + dome * 0.5f);
        const float r = dome * 0.5f;

        if (showLed)
        {
            const float lv = ledLevel >= 0.0f ? juce::jlimit (0.0f, 1.0f, ledLevel) : (getToggleState() ? 1.0f : 0.0f);
            const auto col = theme::ledOff.interpolatedWith (ledColour, 0.25f + 0.75f * lv);
            if (lv > 0.05f)
            {
                g.setColour (ledColour.withAlpha (0.35f * lv));
                g.fillEllipse (c.x - 6.0f, ledY - 3.0f, 12.0f, 12.0f);
            }
            g.setColour (col);
            g.fillEllipse (c.x - 3.0f, ledY, 6.0f, 6.0f);
        }

        if (ring >= 0.0f)
        {
            juce::Path track, prog;
            track.addCentredArc (c.x, c.y, r + 7.0f, r + 7.0f, 0.0f, 0.0f, juce::MathConstants<float>::twoPi, true);
            g.setColour (theme::burn.withAlpha (0.25f));
            g.strokePath (track, juce::PathStrokeType (2.0f));
            prog.addCentredArc (c.x, c.y, r + 7.0f, r + 7.0f, 0.0f, 0.0f, juce::MathConstants<float>::twoPi * juce::jlimit (0.0f, 1.0f, ring), true);
            g.setColour (ringColour);
            g.strokePath (prog, juce::PathStrokeType (2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // recess, dome
        g.setColour (juce::Colour (0x8c281608));
        g.fillEllipse (c.x - r - 3.0f, c.y - r - 3.0f, (r + 3.0f) * 2.0f, (r + 3.0f) * 2.0f);
        g.setColour (juce::Colours::black.withAlpha (0.4f));
        g.fillEllipse (c.x - r, c.y - r + (down ? 0.5f : 2.0f), r * 2.0f, r * 2.0f);
        juce::ColourGradient grad (theme::steelHi, c.x - r * 0.3f, c.y - r * 0.4f, theme::steelLo, c.x + r, c.y + r, true);
        grad.addColour (0.55, theme::steel);
        g.setGradientFill (grad);
        g.fillEllipse (c.x - r, c.y - r + (down ? 1.0f : 0.0f), r * 2.0f, r * 2.0f);
        if (over) { g.setColour (juce::Colours::white.withAlpha (0.12f)); g.fillEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f); }

        if (getButtonText().isNotEmpty())
        {
            const juce::Rectangle<float> tr = labelAbove ? juce::Rectangle<float> (0.0f, 0.0f, w, 13.0f)
                                                         : juce::Rectangle<float> (0.0f, c.y + r + 4.0f, w, 13.0f);
            theme::burnText (g, getButtonText(), tr, juce::Justification::centredTop, theme::font (theme::FontKind::sans, 10.0f, 0.12f), labelColour);
        }
    }

    juce::Colour labelColour = theme::burn;   // bone when the button sits off the wood

private:
    bool held = false;
    void timerCallback() override { stopTimer(); held = true; if (onHold) onHold(); }
};

//==============================================================================
// Burned-text tab for the arc of sections at the top.
class TabButton : public juce::Button
{
public:
    explicit TabButton (const juce::String& t) : juce::Button (t) { setClickingTogglesState (false); }

    void paintButton (juce::Graphics& g, bool over, bool) override
    {
        const auto b = getLocalBounds().toFloat();
        const bool on = getToggleState();
        theme::burnText (g, getButtonText(), b.withTrimmedBottom (6.0f), juce::Justification::centred,
                         theme::font (on ? theme::FontKind::sansBold : theme::FontKind::sans, 13.0f, 0.18f),
                         on || over ? theme::burn : theme::burnSoft);
        if (on)
        {
            g.setColour (theme::burn);
            g.fillRect (b.getCentreX() - 26.0f, b.getBottom() - 6.0f, 52.0f, 2.0f);
        }
    }
};

//==============================================================================
// Brass touch plate. Click and hold to play; drag up for more pressure.
class BrassPlate : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    BrassPlate (const juce::String& labelText, float diameter) : label (labelText), dia (diameter)
    {
        setPaintingIsUnclipped (true);
        setRepaintsOnMouseActivity (false);
    }

    juce::String label, subLabel;
    float dia;
    bool armed = false, locked = false;
    juce::Colour armedColour = theme::amber;
    std::function<void (float)> onPress;
    std::function<void()> onRelease, onRightClick;

    void setLevel (float l)
    {
        if (l > level + 0.25f && l > 0.3f && level < 0.2f) triggerRipple();
        if (std::abs (l - level) > 0.005f) { level = l; repaint(); }
    }
    void triggerRipple() { ripples.push_back (0.0f); }
    bool tick (float dt)
    {
        if (ripples.empty()) return false;
        for (auto& r : ripples) r += dt / 1.6f;
        ripples.erase (std::remove_if (ripples.begin(), ripples.end(), [] (float r) { return r >= 1.0f; }), ripples.end());
        repaint();
        return true;
    }

    void paint (juce::Graphics& g) override
    {
        const juce::Point<float> c ((float) getWidth() * 0.5f, (float) getHeight() * 0.5f);
        const float r = dia * 0.5f;

        // glow when sounding / pressed
        if (level > 0.01f)
        {
            juce::ColourGradient glow (juce::Colour (0xffffc86e).withAlpha (0.75f * level), c.x, c.y,
                                       juce::Colour (0x00ffc86e), c.x + r + 22.0f * level + 6.0f, c.y, true);
            g.setGradientFill (glow);
            const float gr = r + 22.0f * level + 6.0f;
            g.fillEllipse (c.x - gr, c.y - gr, gr * 2.0f, gr * 2.0f);
        }
        for (float age : ripples)
        {
            const float rr = r + age * dia * 0.9f;
            g.setColour (juce::Colour (0xffffd68c).withAlpha (0.85f * (1.0f - age)));
            g.drawEllipse (c.x - rr, c.y - rr, rr * 2.0f, rr * 2.0f, 2.0f * (1.0f - age) + 0.5f);
        }
        if (locked)
        {
            // locked by a double-tap: a twin brass ring
            g.setColour (theme::amber.withAlpha (0.9f));
            g.drawEllipse (c.x - r - 9.0f, c.y - r - 9.0f, (r + 9.0f) * 2.0f, (r + 9.0f) * 2.0f, 2.0f);
            g.drawEllipse (c.x - r - 13.0f, c.y - r - 13.0f, (r + 13.0f) * 2.0f, (r + 13.0f) * 2.0f, 1.0f);
        }
        if (armed)
        {
            g.setColour (armedColour.withAlpha (0.8f));
            g.drawEllipse (c.x - r - 7.0f, c.y - r - 7.0f, (r + 7.0f) * 2.0f, (r + 7.0f) * 2.0f, 2.0f);
        }

        // set into the wood
        g.setColour (juce::Colour (0x8c281608));
        g.fillEllipse (c.x - r - 3.0f, c.y - r - 3.0f, (r + 3.0f) * 2.0f, (r + 3.0f) * 2.0f);
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillEllipse (c.x - r, c.y - r + 3.0f, r * 2.0f, r * 2.0f);

        juce::ColourGradient brass (theme::brassHi, c.x - r * 0.3f, c.y - r * 0.44f, theme::brassDk, c.x + r, c.y + r, true);
        brass.addColour (0.38, theme::brass);
        brass.addColour (0.72, theme::brassLo);
        g.setGradientFill (brass);
        const float press = pressed ? 1.0f : 0.0f;
        g.fillEllipse (c.x - r, c.y - r + press, r * 2.0f, r * 2.0f);
        // brushed rings
        g.setColour (juce::Colour (0x18ffffff));
        for (float k = 0.25f; k < 0.95f; k += 0.12f) g.drawEllipse (c.x - r * k, c.y - r * k + press, r * 2.0f * k, r * 2.0f * k, 0.7f);
        g.setColour (juce::Colour (0x40000000));
        g.drawEllipse (c.x - r + 0.5f, c.y - r + 0.5f + press, r * 2.0f - 1.0f, r * 2.0f - 1.0f, 1.0f);

        g.setColour (juce::Colour (0xff3a240c));
        g.setFont (theme::font (theme::FontKind::sansBold, dia > 50.0f ? 15.0f : 13.0f));
        g.drawText (label, juce::Rectangle<float> (c.x - r, c.y - r + press, r * 2.0f, r * 2.0f), juce::Justification::centred, false);

        if (subLabel.isNotEmpty())
            theme::burnText (g, subLabel, { 0.0f, c.y + r + 5.0f, (float) getWidth(), 13.0f }, juce::Justification::centredTop,
                             theme::font (theme::FontKind::sans, 10.0f, 0.1f));
    }

    bool hitTest (int x, int y) override
    {
        // a little wider than the brass, so a near miss still plays rather than touching the wood
        return juce::Point<float> ((float) getWidth() * 0.5f, (float) getHeight() * 0.5f).getDistanceFrom ({ (float) x, (float) y }) < dia * 0.5f + 8.0f;
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu()) { if (onRightClick) onRightClick(); return; }
        if (WoodTouch::claims (e)) return;   // part of a pinch on the wood
        if (onRightClick && e.source.isTouch()) startTimer (650);   // touch: a long press stands in for right-click
        pressed = true; pressure = 0.7f; startY = e.position.y;
        if (onPress) onPress (pressure);
        repaint();
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! pressed) return;
        if (e.getDistanceFromDragStart() > 8) stopTimer();
        pressure = juce::jlimit (0.05f, 1.0f, 0.7f + (startY - e.position.y) / 120.0f);
        if (onPress) onPress (pressure);
    }
    void mouseUp (const juce::MouseEvent&) override
    {
        stopTimer();
        if (! pressed) return;
        pressed = false;
        if (onRelease) onRelease();
        repaint();
    }

private:
    void timerCallback() override { stopTimer(); if (onRightClick) onRightClick(); }

    float level = 0.0f, pressure = 0.0f, startY = 0.0f;
    bool pressed = false;
    std::vector<float> ripples;
};

//==============================================================================
// The brass porthole in the heart of the tree: the living visuals.
class Porthole : public juce::Component
{
public:
    explicit Porthole (const zen::Display& d) : disp (d)
    {
        setInterceptsMouseClicks (false, false);
        setPaintingIsUnclipped (false);
        for (int i = 0; i < 3; ++i) ambient[i] = { 0.3f + 0.2f * (float) i, (float) i * 2.1f };
    }

    // mode: 0 off, 1 low, 2 full
    void tick (float dt, int mode)
    {
        visMode = mode;
        if (mode == 0) { if (! frozen) { frozen = true; repaint(); } return; }
        frozen = false;
        time += dt;
        rotation += dt * 0.06f;

        // follow the voices
        for (int i = 0; i < zen::Display::maxVoices; ++i)
        {
            auto& b = voice[(size_t) i];
            const float lv = disp.voiceLevel[(size_t) i].load (std::memory_order_relaxed);
            const float pr = disp.voicePress[(size_t) i].load (std::memory_order_relaxed);
            const float pitch = disp.voicePitch[(size_t) i].load (std::memory_order_relaxed);
            b.level += (lv - b.level) * std::min (1.0f, dt * 10.0f);
            b.press += (pr - b.press) * std::min (1.0f, dt * 8.0f);
            if (lv > 0.001f) b.pitch = pitch;
        }

        // the kick drum breathes out from the centre
        if (const int kicks = disp.drumKicks.load(); kicks != lastKicks)
        {
            if (lastKicks >= 0) ripples.push_back ({ { 0.0f, 0.0f }, 0.0f, 0.55f });
            lastKicks = kicks;
        }

        const int count = disp.noteOnCount.load();
        if (count != lastCount)
        {
            const float p = disp.lastPitch.load();
            ripples.push_back ({ positionFor (p, 0.0f), 0.0f, 0.4f + 0.6f * disp.lastVel.load() });
            if (ripples.size() > 24) ripples.erase (ripples.begin());
            lastCount = count;
        }
        for (auto& r : ripples) r.age += dt / (mode == 2 ? 2.2f : 1.4f);
        ripples.erase (std::remove_if (ripples.begin(), ripples.end(), [] (const Ripple& r) { return r.age >= 1.0f; }), ripples.end());

        // the scope: newest 512 samples, phase-aligned on a rising zero crossing
        const int w = disp.scopeWrite.load (std::memory_order_relaxed);
        float buf[zen::Display::scopeSize];
        for (int i = 0; i < zen::Display::scopeSize; ++i)
            buf[i] = disp.scope[(size_t) ((w + i) & (zen::Display::scopeSize - 1))].load (std::memory_order_relaxed);
        int start = zen::Display::scopeSize - 512;
        for (int i = zen::Display::scopeSize - 512; i > 8; --i)
            if (buf[i - 1] <= 0.0f && buf[i] > 0.0f) { start = std::max (0, std::min (i, zen::Display::scopeSize - 384)); break; }
        float peak = 0.02f;
        for (int i = 0; i < scopePts; ++i)
        {
            const float v = buf[std::min (zen::Display::scopeSize - 1, start + i * 3)];
            scope[(size_t) i] += (v - scope[(size_t) i]) * 0.6f;
            peak = std::max (peak, std::abs (scope[(size_t) i]));
        }
        scopeGain += (std::min (3.0f, 0.55f / peak) - scopeGain) * 0.1f;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat();
        const juce::Point<float> c = b.getCentre();
        const float R = b.getWidth() * 0.5f;
        const float ringOuter = R - 12.0f, glassR = R - 24.0f;

        // rotating burned tick ring
        {
            g.setColour (theme::burn.withAlpha (0.75f));
            const int ticks = (int) (R * 0.7f);
            for (int i = 0; i < ticks; ++i)
            {
                const float a = rotation + juce::MathConstants<float>::twoPi * (float) i / (float) ticks;
                const juce::Point<float> p (c.x + (R - 4.0f) * std::sin (a), c.y - (R - 4.0f) * std::cos (a));
                g.fillEllipse (p.x - 1.0f, p.y - 1.6f, 2.0f, 3.2f);
            }
        }
        // brass ring
        g.setColour (juce::Colour (0x8c281608));
        g.fillEllipse (c.x - ringOuter - 3.0f, c.y - ringOuter - 3.0f, (ringOuter + 3.0f) * 2.0f, (ringOuter + 3.0f) * 2.0f);
        juce::ColourGradient brass (theme::brassHi, c.x - ringOuter * 0.3f, c.y - ringOuter * 0.44f, theme::brassDk, c.x + ringOuter, c.y + ringOuter, true);
        brass.addColour (0.38, theme::brass); brass.addColour (0.72, theme::brassLo);
        g.setGradientFill (brass);
        g.fillEllipse (c.x - ringOuter, c.y - ringOuter, ringOuter * 2.0f, ringOuter * 2.0f);

        // the glass
        juce::Path glass; glass.addEllipse (c.x - glassR, c.y - glassR, glassR * 2.0f, glassR * 2.0f);
        g.setColour (theme::glass);
        g.fillPath (glass);
        {
            juce::Graphics::ScopedSaveState ss (g);
            g.reduceClipRegion (glass);
            if (visMode > 0) paintInside (g, c, glassR);
            // inner shadow
            juce::ColourGradient sh (juce::Colours::transparentBlack, c.x, c.y, juce::Colours::black.withAlpha (0.85f), c.x + glassR, c.y, true);
            sh.addColour (0.75, juce::Colours::transparentBlack);
            g.setGradientFill (sh);
            g.fillPath (glass);
            // highlight
            juce::ColourGradient hl (juce::Colours::white.withAlpha (0.16f), c.x - glassR * 0.36f, c.y - glassR * 0.5f,
                                     juce::Colours::transparentWhite, c.x - glassR * 0.36f + glassR * 0.75f, c.y - glassR * 0.5f, true);
            g.setGradientFill (hl);
            g.fillPath (glass);
        }
    }

private:
    struct Blob { float level = 0.0f, press = 0.0f, pitch = 60.0f; };
    struct Ripple { juce::Point<float> pos; float age; float strength; };
    struct Ambient { float speed, phase; };

    const zen::Display& disp;
    static constexpr int scopePts = 128;
    std::array<float, scopePts> scope {};
    float scopeGain = 1.0f;
    std::array<Blob, zen::Display::maxVoices> voice {};
    std::vector<Ripple> ripples;
    Ambient ambient[3];
    float time = 0.0f, rotation = 0.0f;
    int lastCount = 0, visMode = 2, lastKicks = -1;
    bool frozen = false;

    // unit-circle position for a pitch: pitch class sets the angle, octave the distance
    juce::Point<float> positionFor (float pitch, float t) const
    {
        const float pc = std::fmod (std::max (0.0f, pitch), 12.0f);
        const float oct = juce::jlimit (0.0f, 1.0f, (pitch - 36.0f) / 48.0f);
        const float a = juce::MathConstants<float>::twoPi * pc / 12.0f + t * 0.15f;
        const float rr = 0.25f + 0.45f * (1.0f - oct);
        return { rr * std::sin (a), -rr * std::cos (a) };
    }

    static juce::Colour pitchColour (float pitch)
    {
        static const juce::Colour pal[] = { juce::Colour (0xffe8a93e), juce::Colour (0xffd9c27a), juce::Colour (0xff9fbf7a),
                                            juce::Colour (0xff6f9a6a), juce::Colour (0xff8fb5a8), juce::Colour (0xffe7dcc6) };
        const float pc = std::fmod (std::max (0.0f, pitch), 12.0f) / 12.0f * 6.0f;
        const int i = (int) pc;
        return pal[i % 6].interpolatedWith (pal[(i + 1) % 6], pc - (float) i);
    }

    void paintInside (juce::Graphics& g, juce::Point<float> c, float R)
    {
        auto soft = [&] (juce::Point<float> p, float rad, juce::Colour col, float alpha)
        {
            juce::ColourGradient gr (col.withAlpha (alpha), p.x, p.y, col.withAlpha (0.0f), p.x + rad, p.y, true);
            gr.addColour (0.45, col.withAlpha (alpha * 0.55f));
            g.setGradientFill (gr);
            g.fillEllipse (p.x - rad, p.y - rad, rad * 2.0f, rad * 2.0f);
        };

        // ambient drift: honey, moss, bone
        const juce::Colour amb[3] = { theme::honey, theme::moss, juce::Colour (0xffe7dcc6) };
        for (int i = 0; i < 3; ++i)
        {
            const float t = time * ambient[i].speed + ambient[i].phase;
            const juce::Point<float> p (c.x + R * 0.38f * std::sin (t * 0.7f + (float) i * 2.0f), c.y + R * 0.32f * std::cos (t * 0.53f + (float) i));
            soft (p, R * (0.55f + 0.08f * std::sin (t * 1.3f)), amb[i], i == 2 ? 0.22f : 0.38f);
        }

        // one glowing body per sounding voice
        float energy = 0.0f;
        for (auto& v : voice)
        {
            if (v.level < 0.004f) continue;
            energy += v.level;
            const auto u = positionFor (v.pitch, time);
            const juce::Point<float> p (c.x + u.x * R, c.y + u.y * R);
            const float rad = R * (0.18f + 0.28f * v.level + 0.22f * v.press);
            soft (p, rad, pitchColour (v.pitch), juce::jlimit (0.0f, 0.95f, 0.35f + 0.6f * v.level));
            if (visMode == 2)
            {
                g.setColour (juce::Colours::white.withAlpha (0.5f * v.level));
                g.fillEllipse (p.x - 2.5f, p.y - 2.5f, 5.0f, 5.0f);
            }
        }

        // ripples from new notes
        for (auto& r : ripples)
        {
            const juce::Point<float> p (c.x + r.pos.x * R, c.y + r.pos.y * R);
            const float rad = R * (0.08f + 0.9f * r.age);
            g.setColour (theme::glow.withAlpha (0.7f * r.strength * (1.0f - r.age)));
            g.drawEllipse (p.x - rad, p.y - rad, rad * 2.0f, rad * 2.0f, 1.6f + 1.4f * (1.0f - r.age));
            if (visMode == 2 && r.age < 0.6f)
            {
                const float rad2 = rad * 0.6f;
                g.setColour (theme::glow.withAlpha (0.35f * r.strength * (1.0f - r.age / 0.6f)));
                g.drawEllipse (p.x - rad2, p.y - rad2, rad2 * 2.0f, rad2 * 2.0f, 1.0f);
            }
        }

        // the waveform, flowing across the middle
        juce::Path wave;
        for (int i = 0; i < scopePts; ++i)
        {
            const float x = c.x - R + 2.0f * R * (float) i / (float) (scopePts - 1);
            const float edge = std::sin (juce::MathConstants<float>::pi * (float) i / (float) (scopePts - 1));
            const float y = c.y - scope[(size_t) i] * scopeGain * R * 0.42f * edge;
            if (i == 0) wave.startNewSubPath (x, y); else wave.lineTo (x, y);
        }
        const float bright = juce::jlimit (0.25f, 1.0f, 0.35f + energy);
        if (visMode == 2)
        {
            g.setColour (theme::glow.withAlpha (0.18f * bright));
            g.strokePath (wave, juce::PathStrokeType (9.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        g.setColour (theme::glow.withAlpha (0.92f * bright));
        g.strokePath (wave, juce::PathStrokeType (2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
};

//==============================================================================
// Bipolar amount slider for the mod slots: a carved groove with a brass bead.
class AmountSlider : public juce::Slider
{
public:
    AmountSlider()
    {
        setSliderStyle (juce::Slider::LinearHorizontal);
        setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        setDoubleClickReturnValue (true, 0.0);
    }
    juce::Colour colour = theme::amber;

    void paint (juce::Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat().reduced (8.0f, 0.0f);
        const float cy = b.getCentreY() - 3.0f;
        g.setColour (juce::Colour (0x80281608));
        g.fillRoundedRectangle (b.getX(), cy - 3.0f, b.getWidth(), 6.0f, 3.0f);
        const float mid = b.getCentreX();
        const float x = b.getX() + b.getWidth() * (float) valueToProportionOfLength (getValue());
        g.setColour (colour.withAlpha (0.85f));
        g.fillRoundedRectangle (std::min (mid, x), cy - 2.0f, std::abs (x - mid), 4.0f, 2.0f);
        g.setColour (theme::burn.withAlpha (0.6f));
        g.fillRect (mid - 0.5f, cy - 5.0f, 1.0f, 10.0f);
        juce::ColourGradient brass (theme::brassHi, x - 2.0f, cy - 3.0f, theme::brassDk, x + 6.0f, cy + 6.0f, true);
        brass.addColour (0.5, theme::brass);
        g.setGradientFill (brass);
        g.fillEllipse (x - 6.0f, cy - 6.0f, 12.0f, 12.0f);
        theme::burnText (g, getTextFromValue (getValue()), { b.getX(), cy + 7.0f, b.getWidth(), 12.0f }, juce::Justification::centredTop,
                         theme::font (theme::FontKind::sans, 10.0f, 0.08f));
    }
};

//==============================================================================
// A curved touch strip carved along the edge of the slab: PITCH (springs back to the middle)
// or MOD (stays where you leave it). Angles are degrees clockwise from the top of the slab;
// angleBottom is the end nearest the player.
class ArcStrip : public juce::Component, public juce::SettableTooltipClient
{
public:
    ArcStrip (juce::String name, bool springs, juce::Point<float> slabCentre, float radius, float angleBottom, float angleTop)
        : label (std::move (name)), spring (springs), centre (slabCentre), r (radius), aBottom (angleBottom), aTop (angleTop)
    {
        setPaintingIsUnclipped (true);
        // the component covers the band's bounding box (in the parent's coordinates)
        juce::Rectangle<float> box;
        for (int i = 0; i <= 16; ++i)
        {
            const float a = aBottom + (aTop - aBottom) * (float) i / 16.0f;
            for (float rr : { r - 30.0f, r + 18.0f })
                box = box.getUnion (juce::Rectangle<float> (theme::polar (centre, rr, a), theme::polar (centre, rr, a)).expanded (1.0f));
        }
        box = box.expanded (4.0f, 26.0f);
        setBounds (box.getSmallestIntegerContainer());
    }

    std::function<void (float)> onChange;   // pitch: -1..1, mod: 0..1

    // value shown (follows the keyboard too); pitch -1..1, mod 0..1
    void setShown (float v)
    {
        if (dragging || springing) return;
        if (std::abs (v - shown) > 0.004f) { shown = v; repaint(); }
    }

    // call ~60 times a second: lets the pitch strip glide back to the middle
    void tick (float dt)
    {
        if (! springing) return;
        shown *= std::exp (-dt / 0.03f);
        if (std::abs (shown) < 0.003f) { shown = 0.0f; springing = false; }
        if (onChange) onChange (shown);
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const auto c = centre - getPosition().toFloat();
        const float a0 = juce::degreesToRadians (aBottom), a1 = juce::degreesToRadians (aTop);

        // the carved groove
        juce::Path groove;
        groove.addCentredArc (c.x, c.y, r, r, 0.0f, std::min (a0, a1), std::max (a0, a1), true);
        g.setColour (juce::Colour (0x40ffecc8));
        g.strokePath (groove, juce::PathStrokeType (15.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded),
                      juce::AffineTransform::translation (0.0f, 1.0f));
        g.setColour (juce::Colour (0x9c2a1608));
        g.strokePath (groove, juce::PathStrokeType (12.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // tick marks: middle (pitch) or quarters (mod)
        for (int i = 0; i <= 4; ++i)
        {
            if (spring && i != 2) continue;
            const float a = aBottom + (aTop - aBottom) * (float) i / 4.0f;
            const auto p0 = theme::polar (c, r - 11.0f, a), p1 = theme::polar (c, r - 16.0f, a);
            g.setColour (theme::burn.withAlpha (spring ? 0.8f : 0.45f));
            g.drawLine (p0.x, p0.y, p1.x, p1.y, spring ? 2.0f : 1.2f);
        }

        // lit part: from the middle (pitch) or the bottom (mod) to the bead
        const float t = spring ? 0.5f + 0.5f * shown : shown;
        const float aBead = aBottom + (aTop - aBottom) * t;
        const float aFrom = spring ? (aBottom + aTop) * 0.5f : aBottom;
        if (std::abs (aBead - aFrom) > 0.2f)
        {
            juce::Path lit;
            const float x0 = juce::degreesToRadians (std::min (aFrom, aBead)), x1 = juce::degreesToRadians (std::max (aFrom, aBead));
            lit.addCentredArc (c.x, c.y, r, r, 0.0f, x0, x1, true);
            g.setColour (theme::amber.withAlpha (0.85f));
            g.strokePath (lit, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // the brass bead
        const auto b = theme::polar (c, r, aBead);
        if (dragging)
        {
            g.setColour (juce::Colour (0xffffc86e).withAlpha (0.45f));
            g.fillEllipse (b.x - 15.0f, b.y - 15.0f, 30.0f, 30.0f);
        }
        juce::ColourGradient brass (theme::brassHi, b.x - 3.0f, b.y - 4.0f, theme::brassDk, b.x + 8.0f, b.y + 8.0f, true);
        brass.addColour (0.4, theme::brass);
        g.setGradientFill (brass);
        g.fillEllipse (b.x - 9.0f, b.y - 9.0f, 18.0f, 18.0f);
        g.setColour (juce::Colour (0x8c281608));
        g.drawEllipse (b.x - 9.0f, b.y - 9.0f, 18.0f, 18.0f, 1.0f);

        // name, just past the bottom end
        const float past = juce::radiansToDegrees (34.0f / r);   // ~34 px past the end, whatever the radius
        const auto lp = theme::polar (c, r - 4.0f, aBottom + (aBottom > aTop ? past : -past));
        theme::burnText (g, label, juce::Rectangle<float> (60.0f, 14.0f).withCentre (lp), juce::Justification::centred,
                         theme::font (theme::FontKind::sansBold, 10.0f, 0.16f));
    }

    bool hitTest (int x, int y) override
    {
        const auto p = juce::Point<float> ((float) x, (float) y) + getPosition().toFloat();
        const float d = p.getDistanceFrom (centre);
        if (d < r - 28.0f || d > r + 18.0f) return false;
        const float a = angleOf (p);
        return a >= std::min (aBottom, aTop) - 3.0f && a <= std::max (aBottom, aTop) + 3.0f;
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (WoodTouch::claims (e)) return;
        dragging = true; springing = false;
        update (e);
    }
    void mouseDrag (const juce::MouseEvent& e) override { if (dragging) update (e); }
    void mouseUp (const juce::MouseEvent&) override
    {
        if (! dragging) return;
        dragging = false;
        if (spring) springing = true;   // glide back to the middle
        repaint();
    }

private:
    juce::String label;
    bool spring;
    juce::Point<float> centre;
    float r, aBottom, aTop;
    float shown = 0.0f;
    bool dragging = false, springing = false;

    float angleOf (juce::Point<float> p) const
    {
        float a = juce::radiansToDegrees (std::atan2 (p.x - centre.x, centre.y - p.y));
        if (a < 0.0f) a += 360.0f;
        return a;
    }

    void update (const juce::MouseEvent& e)
    {
        const auto p = e.position + getPosition().toFloat();
        const float t = juce::jlimit (0.0f, 1.0f, (angleOf (p) - aBottom) / (aTop - aBottom));
        shown = spring ? t * 2.0f - 1.0f : t;
        if (spring && std::abs (shown) < 0.04f) shown = 0.0f;   // a little dead zone in the middle
        if (onChange) onChange (shown);
        repaint();
    }
};
