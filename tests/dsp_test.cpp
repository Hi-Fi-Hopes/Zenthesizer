// Standalone checks for the Zenthesizer engine (no JUCE).
// Build: g++ -std=c++17 -O2 -I../Source tests/dsp_test.cpp -o /tmp/zt && /tmp/zt

#include "dsp/Engine.h"
#include "dsp/Sandbox.h"
#include <chrono>
#include <cstdio>
#include <random>
#include <vector>

using namespace zen;

static int failures = 0;
#define CHECK(cond, ...) do { if (! (cond)) { ++failures; std::printf ("FAIL: " __VA_ARGS__); std::printf ("\n"); } else { std::printf ("ok:   " __VA_ARGS__); std::printf ("\n"); } } while (0)

struct Run
{
    Engine eng;
    Settings s;
    float fs = 48000.0f;
    std::vector<float> L, R;
    Run() { eng.prepare (fs); s.padNotes = defaultPadNotes(); }

    void render (int samples, std::vector<MidiEvent> ev = {}, int block = 256)
    {
        std::vector<float> l ((size_t) block), r ((size_t) block);
        for (int pos = 0; pos < samples; pos += block)
        {
            const int n = std::min (block, samples - pos);
            std::vector<MidiEvent> here;
            for (auto& e : ev) if (e.offset >= pos && e.offset < pos + n) { auto c = e; c.offset -= pos; here.push_back (c); }
            eng.process (s, here.data(), (int) here.size(), l.data(), r.data(), n);
            L.insert (L.end(), l.begin(), l.begin() + n);
            R.insert (R.end(), r.begin(), r.begin() + n);
        }
    }
};

static MidiEvent on (int at, int note, int vel = 100, int ch = 1)  { return { at, (uint8_t) (0x90 | (ch - 1)), (uint8_t) note, (uint8_t) vel }; }
static MidiEvent off (int at, int note, int ch = 1)                { return { at, (uint8_t) (0x80 | (ch - 1)), (uint8_t) note, 0 }; }
static MidiEvent pat (int at, int note, int v, int ch = 1)          { return { at, (uint8_t) (0xA0 | (ch - 1)), (uint8_t) note, (uint8_t) v }; }

static float rms (const std::vector<float>& x, size_t a, size_t b)
{
    double acc = 0; for (size_t i = a; i < b && i < x.size(); ++i) acc += x[i] * x[i];
    return (float) std::sqrt (acc / (double) std::max<size_t> (1, b - a));
}
static float peak (const std::vector<float>& x) { float p = 0; for (float v : x) p = std::max (p, std::abs (v)); return p; }
static bool finite (const std::vector<float>& x) { for (float v : x) if (! std::isfinite (v)) return false; return true; }

int main()
{
    // 1. default patch, one note
    {
        Run r; r.s.reverbMix = 0.0f;
        r.render (48000, { on (0, 60), off (24000, 60) });
        const float level = rms (r.L, 4800, 24000);
        std::printf ("      single note rms %.3f (%.1f dBFS)\n", level, 20 * std::log10 (level));
        CHECK (level > 0.03f && level < 0.5f, "single note level sane");
        CHECK (rms (r.L, 46000, 48000) < 0.01f, "note releases");
        CHECK (finite (r.L) && finite (r.R), "no NaN");
    }

    // 2. every Engine A wave and every table at low and very high notes
    for (int w = 0; w < 4; ++w)
        for (int note : { 36, 60, 96 })
        {
            Run r; r.s.aWave = (WaveA) w; r.s.aShape = 0.6f; r.s.reverbMix = 0; r.s.cutoff = 1.0f;
            r.render (24000, { on (0, note) });
            CHECK (finite (r.L) && peak (r.L) < 1.25f && rms (r.L, 4000, 24000) > 0.01f, "wave A %d note %d (rms %.3f)", w, note, rms (r.L, 4000, 24000));
        }
    for (int t = 0; t < numTables; ++t)
    {
        Run r; r.s.aLevel = 0; r.s.bLevel = 0.9f; r.s.bTable = t; r.s.bUnison = 0.7f; r.s.bWarp = 0.5f; r.s.reverbMix = 0;
        r.render (24000, { on (0, 48), on (0, 72), on (0, 103) });
        CHECK (finite (r.L) && rms (r.L, 4000, 24000) > 0.01f, "table %s (rms %.3f)", Wavetables::tableName (t), rms (r.L, 4000, 24000));
    }

    // 3. random parameter sweeps + random MIDI: must stay finite and bounded
    {
        Run r;
        std::mt19937 g (7);
        std::uniform_real_distribution<float> u (0, 1);
        std::vector<float> l (512), rr (512);
        bool ok = true; float pk = 0;
        for (int b = 0; b < 48000 * 20 / 512; ++b)
        {
            auto& s = r.s;
            if (b % 8 == 0)
            {
                s.aWave = (WaveA) (g() % 4); s.aTune = u (g); s.aDetune = u (g); s.aShape = u (g); s.aLevel = u (g);
                s.bTable = (int) (g() % 4); s.bPos = u (g); s.bWarp = u (g); s.bUnison = u (g); s.bLevel = u (g); s.air = u (g);
                s.cutoff = u (g); s.res = u (g); s.drive = u (g); s.envAmt = u (g); s.glide = u (g) * 0.5f;
                s.ampA = u (g) * 0.6f; s.ampD = u (g); s.ampS = u (g); s.ampR = u (g) * 0.7f;
                s.chorusMix = u (g); s.delayMix = u (g); s.delayFeedback = u (g); s.reverbMix = u (g); s.reverbShimmer = u (g);
                s.reverbDecay = u (g); s.reverbSize = u (g); s.delaySync = (int) (g() % 7); s.bpm = 60 + 120 * u (g);
                for (auto& sl : s.slots) { sl.src = (Src) (g() % numSrc); sl.dst = (Dst) (g() % numDst); sl.amount = u (g) * 2 - 1; }
                s.arpOn = u (g) < 0.2f; s.arpMode = (ArpMode) (g() % 5);
            }
            std::vector<MidiEvent> ev;
            if (u (g) < 0.3f) ev.push_back (on ((int) (u (g) * 511), 30 + (int) (u (g) * 70), 1 + (int) (u (g) * 126), u (g) < 0.3f ? 10 : 1));
            if (u (g) < 0.3f) ev.push_back (off ((int) (u (g) * 511), 30 + (int) (u (g) * 70), u (g) < 0.3f ? 10 : 1));
            if (u (g) < 0.3f) ev.push_back (pat ((int) (u (g) * 511), 36 + (int) (u (g) * 16), (int) (u (g) * 127), 10));
            std::sort (ev.begin(), ev.end(), [] (auto& a, auto& b2) { return a.offset < b2.offset; });
            r.eng.process (s, ev.data(), (int) ev.size(), l.data(), rr.data(), 512);
            for (int i = 0; i < 512; ++i) { if (! std::isfinite (l[i]) || ! std::isfinite (rr[i])) ok = false; pk = std::max (pk, std::abs (l[i])); }
        }
        CHECK (ok, "20 s random sweep stays finite");
        CHECK (pk <= 1.25f, "random sweep peak bounded (%.2f)", pk);
    }

    // 4. voice stealing: the 9th note must not click
    {
        Run r; r.s.reverbMix = 0; r.s.aWave = WaveA::sine; r.s.aDetune = 0; r.s.ampA = 0.3f; r.s.ampR = 0.6f; r.s.cutoff = 1.0f;
        std::vector<MidiEvent> ev;
        for (int k = 0; k < 8; ++k) ev.push_back (on (k * 1000, 48 + k * 3));
        ev.push_back (on (20000, 90));
        r.render (30000, ev);
        float steady = 0, around = 0;
        for (size_t i = 12001; i < 19000; ++i) steady = std::max (steady, std::abs (r.L[i] - r.L[i - 1]));
        for (size_t i = 19990; i < 20200; ++i) around = std::max (around, std::abs (r.L[i] - r.L[i - 1]));
        std::printf ("      max step steady %.4f, at steal %.4f\n", steady, around);
        CHECK (around < steady * 1.6f + 0.01f, "no click when stealing");
    }

    // 5. plates: pad note 36 on ch 10 plays plate 1; poly AT on 48 drives timbre plate A
    {
        Run r; r.s.reverbMix = 0;
        r.render (9600, { on (0, 36, 100, 10), on (0, 48, 90, 10), pat (2000, 48, 127, 10) });
        CHECK (r.eng.display.lastPlate.load() == 0, "pad 36 -> note plate 1");
        CHECK (std::abs (r.eng.display.lastPitch.load() - (float) plateNote (0, 0, 6, 0)) < 0.01f, "plate 1 pitch = scale root");
        CHECK (r.eng.display.src[(int) Src::plateA].load() > 0.8f, "poly aftertouch -> timbre plate A (%.2f)", r.eng.display.src[(int) Src::plateA].load());
        CHECK (rms (r.L, 4800, 9600) > 0.02f, "plate note sounds");
    }

    // 6. arp: 3 held notes, 1/16 at 120 bpm for 2 s -> 16 steps
    {
        Run r; r.s.arpOn = true; r.s.arpRate = 3; r.s.bpm = 120; r.s.reverbMix = 0;
        r.render (96000, { on (0, 60), on (0, 64), on (0, 67) });
        const int steps = r.eng.display.noteOnCount.load();
        CHECK (steps >= 15 && steps <= 17, "arp 1/16 @120 for 2 s = %d steps", steps);
    }

    // 7. reverb + shimmer tail decays and stays sane
    {
        Run r; r.s.reverbMix = 1.0f; r.s.reverbDecay = 0.6f; r.s.reverbShimmer = 1.0f; r.s.ampR = 0.2f;
        r.render (48000 * 12, { on (0, 60), off (4800, 60) });
        CHECK (finite (r.L) && peak (r.L) < 1.25f, "shimmer reverb bounded (peak %.2f)", peak (r.L));
        CHECK (rms (r.L, 48000 * 11, 48000 * 12) < rms (r.L, 9600, 48000), "tail decays");
    }

    // 8. CPU: 8 voices, everything on, 10 s of audio
    {
        Run r; auto& s = r.s;
        s.aDetune = 0.8f; s.bLevel = 0.8f; s.bUnison = 1.0f; s.bWarp = 0.4f; s.air = 0.5f; s.chorusMix = 0.5f;
        s.delayMix = 0.4f; s.reverbMix = 0.5f; s.reverbShimmer = 0.6f; s.slots[0] = { Src::lfo1, Dst::cutoff, 0.3f };
        std::vector<MidiEvent> ev; for (int k = 0; k < 8; ++k) ev.push_back (on (0, 48 + k * 4));
        auto t0 = std::chrono::steady_clock::now();
        r.render (480000, ev, 512);
        const double sec = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        std::printf ("      CPU: %.2f s to render 10 s (%.1f%% of one core)\n", sec, sec * 10.0);
        CHECK (sec < 3.0, "CPU budget");
    }

    // 9. the sandbox: build a table from keyframes and play it
    {
        using namespace zen::sandbox;
        auto t0 = std::chrono::steady_clock::now();
        std::vector<std::vector<float>> kf { make (Shape::sine), make (Shape::square), make (Shape::noise, 3), make (Shape::organ),
                                             make (Shape::saw), make (Shape::pulse), make (Shape::bell), make (Shape::hollow) };
        const UserTable ut = UserTable::build (kf);
        const double ms = std::chrono::duration<double, std::milli> (std::chrono::steady_clock::now() - t0).count();
        std::printf ("      user table with 8 keyframes built in %.0f ms\n", ms);
        CHECK (ut.frames == 8 && ut.data.size() == (size_t) 8 * 8 * tableSize, "user table shape");

        // the square keyframe, full bandwidth, should be close to the drawn square
        float err = 0.0f;
        const float* sq = ut.view().frame (1, 0);
        for (int n = 40; n < tableSize / 2 - 40; ++n) err = std::max (err, std::abs (sq[n] - 1.0f / 1.09f));
        CHECK (err < 0.2f, "square keyframe survives the band-limit (err %.3f)", err);

        // harmonics round trip
        auto h = Harmonics::of (kf[3]);
        auto back = h.toCycle();
        float rt = 0.0f;
        for (int n = 0; n < tableSize; ++n) rt = std::max (rt, std::abs (back[(size_t) n] - kf[3][(size_t) n]));
        CHECK (rt < 0.01f, "harmonics round trip (err %.4f)", rt);

        // import: a Serum-style file of 64 frames -> 8 keyframes; a 600-sample cycle -> 1
        std::vector<float> wt ((size_t) 64 * 2048);
        for (size_t i = 0; i < wt.size(); ++i) wt[i] = std::sin (twoPi * (float) (i % 2048) / 2048.0f * (float) (1 + i / 2048 / 8));
        CHECK (keyframesFromAudio (wt).size() == 8, "64-frame wavetable -> 8 keyframes");
        std::vector<float> one (600); for (size_t i = 0; i < 600; ++i) one[i] = std::sin (twoPi * (float) i / 600.0f);
        CHECK (keyframesFromAudio (one).size() == 1, "single cycle -> 1 keyframe");
        std::vector<float> longAudio (48000); for (size_t i = 0; i < longAudio.size(); ++i) longAudio[i] = std::sin ((float) i * 0.05f) * std::sin ((float) i * 0.0007f);
        CHECK (keyframesFromAudio (longAudio).size() == 8, "long audio -> 8 slices");

        Run r; r.s.aLevel = 0; r.s.bLevel = 0.9f; r.s.bTable = userTable; r.s.reverbMix = 0; r.s.cutoff = 1.0f;
        r.eng.setUserTable (&ut);
        std::vector<MidiEvent> ev;
        for (int k = 0; k < 6; ++k) ev.push_back (on (k * 4000, 40 + k * 12));
        r.render (48000, ev);
        CHECK (finite (r.L) && peak (r.L) < 1.25f && rms (r.L, 4000, 48000) > 0.02f, "plays the user table (rms %.3f)", rms (r.L, 4000, 48000));

        // a sweep of position across 8 keyframes, with a table swap in the middle
        Run r2; r2.s.aLevel = 0; r2.s.bLevel = 0.9f; r2.s.bTable = userTable; r2.s.reverbMix = 0;
        r2.eng.setUserTable (&ut);
        const UserTable ut2 = UserTable::build ({ make (Shape::triangle) });
        bool ok = true;
        std::vector<float> l (256), rr (256);
        for (int b = 0; b < 400; ++b)
        {
            r2.s.bPos = (float) b / 399.0f;
            if (b == 200) r2.eng.setUserTable (&ut2);
            std::vector<MidiEvent> e2; if (b == 0) e2.push_back (on (0, 57));
            r2.eng.process (r2.s, e2.data(), (int) e2.size(), l.data(), rr.data(), 256);
            for (float v : l) if (! std::isfinite (v) || std::abs (v) > 1.25f) ok = false;
        }
        CHECK (ok, "position sweep + table swap stays clean");
    }

    std::printf (failures == 0 ? "\nALL PASSED\n" : "\n%d FAILED\n", failures);
    return failures == 0 ? 0 : 1;
}
