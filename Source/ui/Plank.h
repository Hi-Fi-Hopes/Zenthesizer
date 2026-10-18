#pragma once

// Renders the landscape layout's board: a long, flat-sawn live-edge plank with straight-cut ends.
// Cathedral grain (the growth rings sliced lengthwise), wane and patches of bark along both long
// edges, end grain on the cut ends, one end check held by a walnut butterfly key.
// Procedural, pixel by pixel, like Wood.h (it reuses Wood.h's noise and ring tables).

#include "Wood.h"

namespace plank
{
    constexpr float designW = 1600.0f, designH = 1000.0f;
    constexpr float endL = 26.0f, endR = 1574.0f;   // the straight cuts

    // live edges (y of the outside of the bark) and how much bark is left there
    inline float topEdge (float x)
    {
        return 44.0f + 10.0f * std::sin (x * 0.0042f + 0.6f) + 6.0f * std::sin (x * 0.011f + 2.0f)
               + 9.0f * (wood::fbm (x * 0.012f, 1.7f, 301, 4) - 0.5f) * 2.0f;
    }
    inline float bottomEdge (float x)
    {
        return 956.0f + 9.0f * std::sin (x * 0.0037f + 2.4f) + 6.0f * std::sin (x * 0.013f + 0.3f)
               + 9.0f * (wood::fbm (x * 0.012f, 5.3f, 307, 4) - 0.5f) * 2.0f;
    }
    // bark thickness: thick in places, gone in others (the wane shows through)
    inline float barkAt (float x, bool top)
    {
        const float n = wood::fbm (x * 0.006f, top ? 11.0f : 23.0f, 313, 3);
        return std::max (0.0f, 30.0f * (n - 0.28f)) + 3.0f * wood::vnoise (x * 0.08f, top ? 3.0f : 9.0f, 317);
    }
    constexpr float waneWidth = 26.0f;   // the rounded, pale sapwood edge where bark has fallen away

    inline juce::Path outline()
    {
        juce::Path p;
        p.startNewSubPath (endL, topEdge (endL));
        for (float x = endL; x <= endR; x += 4.0f) p.lineTo (x, topEdge (x));
        p.lineTo (endR, topEdge (endR));
        p.lineTo (endR, bottomEdge (endR));
        for (float x = endR; x >= endL; x -= 4.0f) p.lineTo (x, bottomEdge (x));
        p.lineTo (endL, bottomEdge (endL));
        p.closeSubPath();
        return p;
    }

    // the butterfly key across the end check on the right
    inline juce::Path butterfly()
    {
        const float x = 1486.0f, y = 902.0f, w = 64.0f, h = 34.0f, waist = 11.0f;
        juce::Path p;
        p.startNewSubPath (x - w * 0.5f, y - h * 0.5f);
        p.lineTo (x, y - waist * 0.5f);
        p.lineTo (x + w * 0.5f, y - h * 0.5f);
        p.lineTo (x + w * 0.5f, y + h * 0.5f);
        p.lineTo (x, y + waist * 0.5f);
        p.lineTo (x - w * 0.5f, y + h * 0.5f);
        p.closeSubPath();
        return p.createPathWithRoundedCorners (2.0f);
    }

    inline juce::Path endCheck()
    {
        juce::Path p;
        std::vector<juce::Point<float>> a, b;
        for (int i = 0; i <= 20; ++i)
        {
            const float x = endR - 150.0f * (float) i / 20.0f;
            const float y = 903.0f + 4.0f * std::sin ((float) i * 0.7f) - 0.03f * (endR - x);
            const float half = 3.2f * (1.0f - (float) i / 20.0f);
            a.push_back ({ x, y - half });
            b.insert (b.begin(), juce::Point<float> (x, y + half));
        }
        p.startNewSubPath (a[0]);
        for (size_t i = 1; i < a.size(); ++i) p.lineTo (a[i]);
        for (auto& q : b) p.lineTo (q);
        p.closeSubPath();
        return p;
    }

    inline juce::Image render (int pixelsWide)
    {
        const float s = (float) pixelsWide / designW;
        const int W = pixelsWide, H = juce::roundToInt (designH * s);
        juce::Image img (juce::Image::ARGB, W, H, true);
        {
            juce::Graphics g (img);
            g.fillAll (theme::table);
            g.addTransform (juce::AffineTransform::scale (s));
            juce::DropShadow (juce::Colours::black.withAlpha (0.75f), 26, { 0, 14 }).drawForPath (g, outline());
        }

        // the edges, tabulated across the board's length
        const int N = 2048;
        std::vector<float> tTop (N + 1), tBot (N + 1), bTop (N + 1), bBot (N + 1);
        for (int i = 0; i <= N; ++i)
        {
            const float x = endL + (endR - endL) * (float) i / (float) N;
            tTop[(size_t) i] = topEdge (x); tBot[(size_t) i] = bottomEdge (x);
            bTop[(size_t) i] = barkAt (x, true); bBot[(size_t) i] = barkAt (x, false);
        }
        auto look = [&] (const std::vector<float>& t, float x)
        {
            const float f = juce::jlimit (0.0f, (float) N - 0.001f, (x - endL) / (endR - endL) * (float) N);
            const int i = (int) f;
            return t[(size_t) i] + (t[(size_t) i + 1] - t[(size_t) i]) * (f - (float) i);
        };

        const auto& ringR = wood::rings();
        std::vector<float> ringStrength (ringR.size());
        for (size_t k = 0; k < ringR.size(); ++k)
            ringStrength[k] = (0.55f + 0.9f * wood::hash2 ((int) k, 7, 99)) * (((k % 4) == 0 || (k % 9) == 0) ? 1.45f : 1.0f);

        juce::Image::BitmapData bd (img, juce::Image::BitmapData::readWrite);
        const juce::Colour barkDark (0xff120a05), barkMid (0xff3a2414), barkLight (0xff6a4426);
        const juce::Colour late (0xff6b3f1c), sap (0xfff0dcb4), waneCol (0xffd9bf95), lightCol (0xfffff6e4), endCol (0xff8a5a30);

        auto renderRows = [&] (int row0, int step)
        {
            for (int py = row0; py < H; py += step)
                for (int px = 0; px < W; ++px)
                {
                    const float x = ((float) px + 0.5f) / s, y = ((float) py + 0.5f) / s;
                    if (x < endL - 1.0f || x > endR + 1.0f) continue;
                    const float yt = look (tTop, x), yb = look (tBot, x);
                    // anti-aliased edges: live edges top and bottom, straight cuts at the ends
                    const float aEdge = juce::jlimit (0.0f, 1.0f, std::min ({ (y - yt) * s + 0.5f, (yb - y) * s + 0.5f,
                                                                               (x - endL) * s + 0.5f, (endR - x) * s + 0.5f }));
                    if (aEdge <= 0.0f) continue;

                    const float barkT = look (bTop, x), barkB = look (bBot, x);
                    const float fromTop = y - yt, fromBot = yb - y;
                    juce::Colour c;

                    if (fromTop < barkT || fromBot < barkB)
                    {
                        // bark: fissures run along the board
                        const float n = wood::fbm (x * 0.05f, y * 0.35f, 5, 3);
                        const float fis = juce::jlimit (0.0f, 1.0f, (n - 0.28f) * 1.7f);
                        c = wood::mix (barkDark, wood::mix (barkMid, barkLight, wood::vnoise (x * 0.12f, y * 0.6f, 8)), fis * fis);
                        const float d = fromTop < barkT ? fromTop / std::max (1.0f, barkT) : fromBot / std::max (1.0f, barkB);
                        c = c.darker (0.35f * (1.0f - d));
                    }
                    else
                    {
                        // cathedral grain: the log's rings, sliced by the board's face. The pith runs along
                        // the board below the face and wanders, so the rings come up as long arches.
                        const float pithY = 505.0f + 30.0f * std::sin (x * 0.0021f + 0.5f) + 10.0f * std::sin (x * 0.0063f);
                        // the log tapers, so the pith sinks below the face toward one end: nested arches
                        const float depth = 26.0f + 0.12f * (x - endL) + 22.0f * std::sin (x * 0.0043f + 0.4f);
                        const float dy = (y - pithY) * 1.02f;
                        const float d = std::sqrt (dy * dy + depth * depth) + 3.0f * (wood::vnoise (x * 0.012f, y * 0.05f, 13) - 0.5f);

                        auto it = std::upper_bound (ringR.begin(), ringR.end(), d);
                        const int k = (int) (it - ringR.begin()) - 1;
                        float frac = 0.5f, strength = 1.0f;
                        if (k >= 0 && k + 1 < (int) ringR.size())
                        {
                            frac = (d - ringR[(size_t) k]) / (ringR[(size_t) k + 1] - ringR[(size_t) k]);
                            strength = ringStrength[(size_t) k];
                        }
                        const float lw = juce::jlimit (0.0f, 1.0f, (frac - 0.5f) / 0.45f);
                        const float lateAmt = lw * lw * (3.0f - 2.0f * lw) * 0.38f * strength;
                        const float earlyLift = frac < 0.12f ? 0.06f * (1.0f - frac / 0.12f) : 0.0f;

                        c = wood::heartwood (juce::jmin (1.0f, d / 600.0f));
                        c = wood::mix (c, late, lateAmt);
                        c = c.withMultipliedBrightness (1.0f + earlyLift);

                        // long fibres and slow stains
                        const float fiber = wood::vnoise (x * 0.018f, y * 0.9f, 33) - 0.5f;
                        const float grain = wood::vnoise (x * 0.25f, y * 1.4f, 21) - 0.5f;
                        const float stain = wood::vnoise (x * 0.004f, y * 0.007f, 41) - 0.5f;
                        c = c.withMultipliedBrightness (1.0f + 0.08f * fiber + 0.05f * grain + 0.12f * stain);

                        // the wane: rounded, pale sapwood where the bark has come away
                        const float toEdge = std::min (fromTop - barkT, fromBot - barkB);
                        if (toEdge < waneWidth)
                        {
                            const float w = 1.0f - toEdge / waneWidth;
                            c = wood::mix (c, sap, 0.45f * w);
                            c = wood::mix (c, waneCol.darker (0.25f), 0.35f * w * w);   // it curves away from the light
                        }

                        // end grain on the straight cuts, and faint saw marks
                        const float toEnd = std::min (x - endL, endR - x);
                        if (toEnd < 9.0f)
                        {
                            const float e = 1.0f - toEnd / 9.0f;
                            c = wood::mix (c, endCol, 0.55f * e);
                            c = c.withMultipliedBrightness (1.0f - 0.08f * e * (0.5f + 0.5f * std::sin (y * 0.9f)));
                        }

                        // light from the top left
                        const float lx = (x - 420.0f) / 1.6f, ly = y - 250.0f;
                        const float light = juce::jlimit (0.0f, 1.0f, 1.0f - std::sqrt (lx * lx + ly * ly) / 640.0f);
                        c = wood::mix (c, lightCol, 0.18f * light);
                    }

                    const juce::Colour under = bd.getPixelColour (px, py);
                    bd.setPixelColour (px, py, under.interpolatedWith (c, aEdge));
                }
        };

        const int threads = juce::jlimit (1, 16, (int) std::thread::hardware_concurrency());
        std::vector<std::thread> pool;
        for (int i = 1; i < threads; ++i) pool.emplace_back (renderRows, i, threads);
        renderRows (0, threads);
        for (auto& th : pool) th.join();

        // the end check and its butterfly key (walnut)
        juce::Graphics g (img);
        g.addTransform (juce::AffineTransform::scale (s));
        g.setColour (juce::Colour (0xff2a170a).withAlpha (0.88f));
        g.fillPath (endCheck());
        const auto bf = butterfly();
        juce::DropShadow (juce::Colours::black.withAlpha (0.35f), 3, { 0, 1 }).drawForPath (g, bf);
        const auto bb = bf.getBounds();
        juce::ColourGradient walnut (juce::Colour (0xff4a2c18), bb.getX(), bb.getY(), juce::Colour (0xff2b170c), bb.getRight(), bb.getBottom(), false);
        walnut.addColour (0.5, juce::Colour (0xff3b2213));
        g.setGradientFill (walnut);
        g.fillPath (bf);
        g.setColour (juce::Colour (0x22ffffff));
        for (int i = 0; i < 7; ++i)
            g.drawLine (bb.getX(), bb.getY() + 6.0f + 4.5f * (float) i, bb.getRight(), bb.getY() + 5.0f + 4.5f * (float) i + std::sin ((float) i) * 2.0f, 0.6f);
        g.setColour (juce::Colour (0xff1a0d05).withAlpha (0.6f));
        g.strokePath (bf, juce::PathStrokeType (1.0f));
        return img;
    }
}
