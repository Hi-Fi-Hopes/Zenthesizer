#pragma once

// Renders the cross-cut tree slab (growth rings, rays, heartwood, sapwood, bark, cracks)
// into an image once. Everything here is procedural, pixel by pixel.

#include "Theme.h"
#include <cmath>
#include <thread>
#include <vector>

namespace wood
{
    constexpr float designSize = 980.0f;
    constexpr float cx = 490.0f, cy = 490.0f, outerR = 452.0f;
    constexpr float pithX = cx - 14.0f, pithY = cy + 12.0f;

    // ---- noise -----------------------------------------------------------------
    inline float hash2 (int x, int y, int seed)
    {
        uint32_t h = (uint32_t) x * 374761393u + (uint32_t) y * 668265263u + (uint32_t) seed * 2246822519u;
        h = (h ^ (h >> 13)) * 1274126177u;
        h ^= h >> 16;
        return (float) (h & 0xffffff) / 16777215.0f;
    }
    inline float vnoise (float x, float y, int seed)
    {
        const int xi = (int) std::floor (x), yi = (int) std::floor (y);
        const float fx = x - (float) xi, fy = y - (float) yi;
        const float u = fx * fx * (3.0f - 2.0f * fx), v = fy * fy * (3.0f - 2.0f * fy);
        const float a = hash2 (xi, yi, seed), b = hash2 (xi + 1, yi, seed);
        const float c = hash2 (xi, yi + 1, seed), d = hash2 (xi + 1, yi + 1, seed);
        return a + (b - a) * u + (c - a) * v + (a - b - c + d) * u * v;
    }
    inline float fbm (float x, float y, int seed, int oct)
    {
        float sum = 0.0f, amp = 0.5f, norm = 0.0f;
        for (int o = 0; o < oct; ++o)
        {
            sum += amp * vnoise (x, y, seed + o * 31);
            norm += amp; amp *= 0.5f; x *= 2.03f; y *= 2.03f;
        }
        return sum / norm;
    }

    // ---- shape of the slab -------------------------------------------------------
    // angle t: 0 = top, clockwise (radians)
    inline float wobble (float t)  { return 5.0f * std::sin (3.0f * t + 0.7f) + 3.0f * std::sin (5.0f * t + 2.1f) + 2.0f * std::sin (9.0f * t + 0.4f); }
    inline float barkThickness (float t) { return 24.0f + 9.0f * std::sin (4.0f * t + 1.0f) + 6.0f * std::sin (11.0f * t + 3.0f) + 3.0f * std::sin (23.0f * t); }
    inline float outerAt (float t)
    {
        const float jag = 2.5f * std::sin (37.0f * t) + 1.8f * std::sin (61.0f * t + 1.0f)
                        + 5.0f * (fbm (t * 40.0f, 3.1f, 91, 3) - 0.5f) * 2.0f;
        return outerR + wobble (t) + jag;
    }
    inline float innerAt (float t)
    {
        return outerR + wobble (t) - barkThickness (t) + 2.0f * (fbm (t * 70.0f, 7.7f, 17, 2) - 0.5f);
    }

    inline juce::Path innerPath()
    {
        juce::Path p;
        for (int i = 0; i <= 720; ++i)
        {
            const float t = juce::MathConstants<float>::twoPi * (float) i / 720.0f;
            const float r = innerAt (t);
            const juce::Point<float> pt (cx + r * std::sin (t), cy - r * std::cos (t));
            if (i == 0) p.startNewSubPath (pt); else p.lineTo (pt);
        }
        p.closeSubPath();
        return p;
    }

    inline juce::Path outerPath()
    {
        juce::Path p;
        for (int i = 0; i <= 720; ++i)
        {
            const float t = juce::MathConstants<float>::twoPi * (float) i / 720.0f;
            const float r = outerAt (t);
            const juce::Point<float> pt (cx + r * std::sin (t), cy - r * std::cos (t));
            if (i == 0) p.startNewSubPath (pt); else p.lineTo (pt);
        }
        p.closeSubPath();
        return p;
    }

    // Growth ring radii (years), wide near the pith, tighter near the bark.
    inline const std::vector<float>& rings()
    {
        static const std::vector<float> r = []
        {
            std::vector<float> out;
            float rr = 5.0f; int k = 0;
            while (rr < 460.0f)
            {
                out.push_back (rr); ++k;
                rr += std::max (3.0f, 7.0f + 3.5f * std::sin ((float) k * 1.3f) + 2.5f * std::sin ((float) k * 0.47f) - (rr > 300.0f ? 2.5f : 0.0f));
            }
            return out;
        }();
        return r;
    }

    inline juce::Colour mix (juce::Colour a, juce::Colour b, float t) { return a.interpolatedWith (b, juce::jlimit (0.0f, 1.0f, t)); }

    inline juce::Colour heartwood (float rr)   // rr = radius / 440
    {
        static const juce::Colour c0 (0xffb48150), c1 (0xffc79865), c2 (0xffdbb788), c3 (0xffead0a8);
        if (rr < 0.55f) return mix (c0, c1, rr / 0.55f);
        if (rr < 0.80f) return mix (c1, c2, (rr - 0.55f) / 0.25f);
        return mix (c2, c3, (rr - 0.80f) / 0.20f);
    }

    // The radial cracks (checks) where the slab dried and split.
    inline juce::Path cracks()
    {
        juce::Path p;
        auto crack = [&] (float deg, float r0, float r1, float w)
        {
            std::vector<juce::Point<float>> left, right;
            for (int i = 0; i <= 16; ++i)
            {
                const float rr = r1 - (r1 - r0) * (float) i / 16.0f;
                const float half = w * (rr - r0) / (r1 - r0) * 0.5f;
                const float t = juce::degreesToRadians (deg + 0.9f * std::sin (rr * 0.07f));
                const float nx = std::cos (t), ny = std::sin (t);
                const float px = cx + rr * std::sin (t), py = cy - rr * std::cos (t);
                left.push_back ({ px - nx * half, py - ny * half });
                right.insert (right.begin(), juce::Point<float> (px + nx * half, py + ny * half));
            }
            p.startNewSubPath (left[0]);
            for (size_t i = 1; i < left.size(); ++i) p.lineTo (left[i]);
            for (auto& q : right) p.lineTo (q);
            p.closeSubPath();
        };
        crack (24.0f, 364.0f, 446.0f, 7.0f);   // stops short of the tabs
        crack (108.0f, 404.0f, 446.0f, 6.0f);   // short checks near the bark, clear of the controls
        crack (-108.0f, 400.0f, 446.0f, 8.0f);
        return p;
    }

    // ---- the render ----------------------------------------------------------------
    inline juce::Image render (int pixels)
    {
        const float s = (float) pixels / designSize;   // pixels per design unit
        juce::Image img (juce::Image::ARGB, pixels, pixels, true);
        {
            juce::Graphics g (img);
            g.fillAll (theme::table);
            g.addTransform (juce::AffineTransform::scale (s));
            juce::DropShadow (juce::Colours::black.withAlpha (0.75f), 26, { 0, 14 }).drawForPath (g, outerPath());
        }

        // the edge, tabulated by angle (the noise in it is the slow part)
        constexpr int N = 4096;
        std::vector<float> tabOuter (N + 1), tabInner (N + 1);
        for (int i = 0; i <= N; ++i)
        {
            const float t = juce::MathConstants<float>::twoPi * (float) i / (float) N;
            tabOuter[(size_t) i] = outerAt (t);
            tabInner[(size_t) i] = innerAt (t);
        }
        auto lookup = [&] (const std::vector<float>& tab, float t)
        {
            const float x = t / juce::MathConstants<float>::twoPi * (float) N;
            const int i = juce::jlimit (0, N - 1, (int) x);
            const float f = x - (float) i;
            return tab[(size_t) i] + (tab[(size_t) i + 1] - tab[(size_t) i]) * f;
        };

        const auto& ringR = rings();
        std::vector<float> ringStrength (ringR.size());
        for (size_t k = 0; k < ringR.size(); ++k)
            ringStrength[k] = (0.55f + 0.9f * hash2 ((int) k, 7, 99)) * (((k % 4) == 0 || (k % 9) == 0) ? 1.45f : 1.0f);

        juce::Image::BitmapData bd (img, juce::Image::BitmapData::readWrite);
        const juce::Colour barkDark (0xff120a05), barkMid (0xff3a2414), barkLight (0xff6a4426);
        const juce::Colour late (0xff6b3f1c), cambium (0xfff6e6c4), sap (0xfff0dcb4), lipShadow (0xff2a160a), lightCol (0xfffff6e4);

        auto renderRows = [&] (int row0, int step)
        {
            for (int py = row0; py < pixels; py += step)
            {
                for (int px = 0; px < pixels; ++px)
                {
                    const float x = ((float) px + 0.5f) / s, y = ((float) py + 0.5f) / s;
                    const float dx = x - cx, dy = y - cy;
                    const float r = std::sqrt (dx * dx + dy * dy);
                    if (r > outerR + 30.0f) continue;
                    float t = std::atan2 (dx, -dy); if (t < 0) t += juce::MathConstants<float>::twoPi;

                    const float ro = lookup (tabOuter, t);
                    const float edgeA = juce::jlimit (0.0f, 1.0f, (ro - r) * s + 0.5f);   // anti-aliased outer edge
                    if (edgeA <= 0.0f) continue;
                    const float ri = lookup (tabInner, t);

                    juce::Colour c;
                    if (r > ri)
                    {
                        // bark: fissures run outward, so stretch the noise along the radius
                        const float u = t * 260.0f, v = r * 0.09f;
                        const float n = fbm (u * 0.35f, v, 5, 3);
                        const float fis = juce::jlimit (0.0f, 1.0f, (n - 0.28f) * 1.7f);
                        c = mix (barkDark, mix (barkMid, barkLight, vnoise (u * 0.9f, v * 3.0f, 8)), fis * fis);
                        const float depth = (r - ri) / std::max (1.0f, ro - ri);
                        c = c.darker (0.35f * depth);
                        if (r - ri < 2.5f) c = mix (c, cambium, 0.75f * (1.0f - (r - ri) / 2.5f));
                    }
                    else
                    {
                        // wood: growth rings around an off-centre pith
                        const float pdx = x - pithX, pdy = y - pithY;
                        const float pt = std::atan2 (pdx, -pdy);
                        const float w = 1.0f + 0.03f * std::sin (2.0f * pt + 0.8f) + 0.022f * std::sin (3.0f * pt + 2.2f);
                        const float pr = std::sqrt (pdx * pdx + pdy * pdy);
                        const float d = pr / w + 2.2f * (vnoise (x * 0.04f, y * 0.04f, 13) - 0.5f);

                        auto it = std::upper_bound (ringR.begin(), ringR.end(), d);
                        const int k = (int) (it - ringR.begin()) - 1;
                        float frac = 0.5f, strength = 1.0f;
                        if (k >= 0 && k + 1 < (int) ringR.size())
                        {
                            frac = (d - ringR[(size_t) k]) / (ringR[(size_t) k + 1] - ringR[(size_t) k]);
                            strength = ringStrength[(size_t) k];
                        }
                        // earlywood is pale, latewood darkens toward the end of each year, then the next year starts pale
                        const float lw = juce::jlimit (0.0f, 1.0f, (frac - 0.5f) / 0.45f);
                        const float lateAmt = lw * lw * (3.0f - 2.0f * lw) * 0.30f * strength;
                        const float earlyLift = frac < 0.12f ? 0.06f * (1.0f - frac / 0.12f) : 0.0f;

                        c = heartwood (d / 440.0f);
                        c = mix (c, late, lateAmt);
                        c = c.withMultipliedBrightness (1.0f + earlyLift);

                        // faint, broken medullary rays
                        float ray = std::abs (std::sin (pt * 48.0f + 3.0f * vnoise (d * 0.015f, pt * 2.0f, 4)));
                        ray *= ray; ray *= ray; ray *= ray; ray *= ray; ray *= ray; ray *= ray;   // ^64
                        c = c.darker (0.05f * ray * vnoise (d * 0.05f, pt * 6.0f, 6));

                        // grain, fibres along the rings, slow stains
                        const float grain = vnoise (x * 0.9f, y * 0.9f, 21) - 0.5f;
                        const float fiber = vnoise (pt * 40.0f, d * 1.3f, 33) - 0.5f;
                        const float stain = vnoise (x * 0.007f, y * 0.007f, 41) - 0.5f;
                        c = c.withMultipliedBrightness (1.0f + 0.09f * grain + 0.06f * fiber + 0.12f * stain);

                        // the pith
                        if (pr < 5.0f) c = mix (c, late.darker (0.4f), 0.6f * (1.0f - pr / 5.0f));

                        // sapwood glow near the bark, a shadow right under the bark's lip
                        const float toEdge = ri - r;
                        if (toEdge < 22.0f) c = mix (c, sap, 0.32f * (1.0f - toEdge / 22.0f));
                        if (toEdge < 5.0f)  c = mix (c, lipShadow, 0.42f * (1.0f - toEdge / 5.0f));

                        // light from the top left
                        const float lx = x - 330.0f, ly = y - 260.0f;
                        const float light = juce::jlimit (0.0f, 1.0f, 1.0f - std::sqrt (lx * lx + ly * ly) / 620.0f);
                        c = mix (c, lightCol, 0.2f * light);
                    }

                    const juce::Colour under = bd.getPixelColour (px, py);
                    bd.setPixelColour (px, py, under.interpolatedWith (c, edgeA));
                }
            }
        };

        const int threads = juce::jlimit (1, 16, (int) std::thread::hardware_concurrency());
        std::vector<std::thread> pool;
        for (int i = 1; i < threads; ++i) pool.emplace_back (renderRows, i, threads);
        renderRows (0, threads);
        for (auto& th : pool) th.join();

        juce::Graphics g (img);
        g.addTransform (juce::AffineTransform::scale (s));
        g.saveState();
        g.reduceClipRegion (innerPath());
        g.setColour (juce::Colour (0xff2a170a).withAlpha (0.88f));
        g.fillPath (cracks());
        g.restoreState();
        return img;
    }
}
