#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "BinaryData.h"

// Colours, fonts and the burned-in text style of the tree slab.
namespace theme
{
    inline const juce::Colour table   { 0xff14100c };   // the dark tabletop around the slab
    inline const juce::Colour burn    { 0xff3b1f0c };   // pyrography brown
    inline const juce::Colour burnSoft{ 0xff5a3418 };
    inline const juce::Colour brassHi { 0xfffff1c2 };
    inline const juce::Colour brass   { 0xffd9a94a };
    inline const juce::Colour brassLo { 0xff9a6a1f };
    inline const juce::Colour brassDk { 0xff6e4a14 };
    inline const juce::Colour steelHi { 0xffffffff };
    inline const juce::Colour steel   { 0xffb8bcc2 };
    inline const juce::Colour steelLo { 0xff6c7077 };
    inline const juce::Colour walnutHi{ 0xff8a6240 };
    inline const juce::Colour walnut  { 0xff4a2e18 };
    inline const juce::Colour walnutLo{ 0xff2e1a0b };
    inline const juce::Colour glass   { 0xff0d0906 };
    inline const juce::Colour glow    { 0xfff3d9a4 };   // warm light in the screens
    inline const juce::Colour ledOff  { 0xff5a3a1a };
    inline const juce::Colour amber   { 0xffffb347 };
    inline const juce::Colour red     { 0xffff5a3c };
    inline const juce::Colour green   { 0xff8be07a };
    inline const juce::Colour blue    { 0xff7cc7ff };
    inline const juce::Colour bone    { 0xfff0e6d0 };
    inline const juce::Colour moss    { 0xff6f9a6a };
    inline const juce::Colour honey   { 0xffe8a93e };

    // One colour per modulation source (index = zen::Src).
    inline juce::Colour srcColour (int src)
    {
        static const juce::Colour c[] = {
            ledOff, red, amber, blue, green, bone, juce::Colour (0xffffd27a), juce::Colour (0xffff9ec4),
            juce::Colour (0xffc7b8ff), juce::Colour (0xff9cf0c2),
            juce::Colour (0xffffc46b), juce::Colour (0xff7fe0d0), juce::Colour (0xffd9a0ff), juce::Colour (0xffa0d4ff),
            juce::Colour (0xffe0e0a0), juce::Colour (0xff8fd6ff) };
        return c[juce::jlimit (0, 15, src)];
    }

    enum class FontKind { serif, sans, sansBold };

    inline juce::Typeface::Ptr typeface (FontKind k)
    {
        static auto serif = juce::Typeface::createSystemTypefaceFor (BinaryData::InstrumentSerifItalic_ttf, BinaryData::InstrumentSerifItalic_ttfSize);
        static auto sans  = juce::Typeface::createSystemTypefaceFor (BinaryData::InstrumentSansMedium_ttf, BinaryData::InstrumentSansMedium_ttfSize);
        static auto bold  = juce::Typeface::createSystemTypefaceFor (BinaryData::InstrumentSansSemiBold_ttf, BinaryData::InstrumentSansSemiBold_ttfSize);
        return k == FontKind::serif ? serif : k == FontKind::sans ? sans : bold;
    }

    inline juce::Font font (FontKind k, float height, float tracking = 0.0f)
    {
        juce::Font f { juce::FontOptions (typeface (k)).withHeight (height) };
        return f.withExtraKerningFactor (tracking);
    }

    // Burned-in text: a soft scorch halo, then the crisp letters.
    inline void burnText (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> r, juce::Justification j,
                          const juce::Font& f, juce::Colour c = burn)
    {
        g.setFont (f);
        g.setColour (c.withAlpha (0.18f));
        for (auto d : { juce::Point<float> (-0.7f, 0.0f), { 0.7f, 0.0f }, { 0.0f, -0.7f }, { 0.0f, 0.7f } })
            g.drawText (text, r.translated (d.x, d.y), j, false);
        g.setColour (c);
        g.drawText (text, r, j, false);
    }

    // Text along a circle. angle: 0 = top, clockwise, in radians. On the top half the letters stand
    // outward from the centre; with bottom = true they read left-to-right along the bottom, standing inward.
    inline void arcText (juce::Graphics& g, const juce::String& text, juce::Point<float> centre, float radius,
                         float angle, const juce::Font& f, juce::Colour c = burn, bool bottom = false, bool scorch = true)
    {
        juce::GlyphArrangement ga;
        ga.addLineOfText (f, text, 0.0f, 0.0f);
        const float total = ga.getBoundingBox (0, -1, true).getWidth();

        auto drawPass = [&] (juce::Colour col, juce::Point<float> nudge)
        {
            g.setColour (col);
            for (int i = 0; i < ga.getNumGlyphs(); ++i)
            {
                const auto& gl = ga.getGlyph (i);
                const float mid = gl.getLeft() + gl.getBounds().getWidth() * 0.5f - total * 0.5f;
                const float a = bottom ? angle - mid / radius : angle + mid / radius;
                const juce::Point<float> pos (centre.x + radius * std::sin (a), centre.y - radius * std::cos (a));
                const float rot = bottom ? a - juce::MathConstants<float>::pi : a;
                auto t = juce::AffineTransform::translation (gl.getLeft() - (gl.getLeft() + gl.getBounds().getWidth() * 0.5f) + nudge.x, nudge.y)
                             .rotated (rot).translated (pos);
                juce::GlyphArrangement one;
                one.addGlyph (gl);
                one.moveRangeOfGlyphs (0, 1, -gl.getLeft(), -gl.getBaselineY());
                one.draw (g, t.translated (0, 0));
            }
        };
        if (scorch)
            for (auto d : { juce::Point<float> (-0.7f, 0.0f), { 0.7f, 0.0f }, { 0.0f, -0.7f }, { 0.0f, 0.7f } })
                drawPass (c.withAlpha (0.16f), d);
        drawPass (c, {});
    }

    // Polar helper: angle 0 = top, clockwise, degrees.
    inline juce::Point<float> polar (juce::Point<float> c, float r, float deg)
    {
        const float a = juce::degreesToRadians (deg);
        return { c.x + r * std::sin (a), c.y - r * std::cos (a) };
    }
}
