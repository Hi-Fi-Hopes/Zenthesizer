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
    for (int w = 0; w < 5; ++w)
        for (int note : { 36, 60, 96 })
        {
            Run r; r.s.aWave = (WaveA) w; r.s.aShape = 0.6f; r.s.reverbMix = 0; r.s.cutoff = 1.0f;
            r.render (24000, { on (0, note) });
            // (a plucked string rings down, quickly up high, so look at its first quarter second)
            const float lv = w == (int) WaveA::string ? rms (r.L, 200, 12000) : rms (r.L, 4000, 24000);
            CHECK (finite (r.L) && peak (r.L) < 1.25f && lv > 0.01f, "wave A %d note %d (rms %.3f)", w, note, lv);
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
                s.aWave = (WaveA) (g() % 5); s.aTune = u (g); s.aDetune = u (g); s.aShape = u (g); s.aLevel = u (g);
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

    // 10. looper: record a pad, change the sound completely, the loop keeps playing as recorded
    {
        Run r; r.s.reverbMix = 0; r.s.aWave = WaveA::sine; r.s.aDetune = 0; r.s.cutoff = 1.0f; r.s.ampA = 0.2f; r.s.ampR = 0.2f;
        r.eng.looper.requestLoop();                                  // start recording
        r.render (48000, { on (0, 57), off (40000, 57) });
        r.eng.looper.requestLoop();                                  // close: 1 s loop, playing
        r.render (2400);
        CHECK (r.eng.looper.getState() == Looper::playing, "looper playing after the second press");
        CHECK (std::abs (r.eng.looper.length() - 48000) < 1400, "loop length ~1 s (%d)", r.eng.looper.length());
        // "switch patch": silence the synth entirely; the loop must still sound
        r.s.aLevel = 0; r.s.bLevel = 0;
        r.L.clear(); r.R.clear();
        r.render (48000);
        const float loopRms = rms (r.L, 0, 48000);
        CHECK (loopRms > 0.03f, "loop keeps playing after the sound changes (rms %.3f)", loopRms);
        float jump = 0; for (size_t i = 1; i < r.L.size(); ++i) jump = std::max (jump, std::abs (r.L[i] - r.L[i - 1]));
        CHECK (jump < 0.2f, "no click at the loop point (max step %.3f)", jump);

        // overdub a second note, then undo it
        r.s.aLevel = 0.8f;
        r.eng.looper.requestLoop();                                  // overdub
        r.L.clear(); r.render (48000, { on (0, 64), off (30000, 64) });
        r.eng.looper.requestLoop();                                  // back to play
        r.s.aLevel = 0;
        r.L.clear(); r.render (48000);
        const float withOverdub = rms (r.L, 0, 48000);
        r.eng.looper.requestUndo();
        r.L.clear(); r.render (48000);
        const float afterUndo = rms (r.L, 0, 48000);
        std::printf ("      loop rms: first %.3f, with overdub %.3f, after undo %.3f\n", loopRms, withOverdub, afterUndo);
        CHECK (withOverdub > loopRms * 1.1f && std::abs (afterUndo - loopRms) < loopRms * 0.15f, "overdub adds, undo takes it back");

        r.eng.looper.requestStop();
        r.L.clear(); r.render (9600);
        CHECK (rms (r.L, 4800, 9600) < 1.0e-3f, "stop silences the loop");
        r.eng.looper.requestClear();
        r.render (512);
        CHECK (r.eng.looper.getState() == Looper::empty, "clear empties it");
    }

    // 11. double-tap a timbre plate (pad 48 = plate A) to lock it, double-tap again to unlock
    {
        Run r; r.s.reverbMix = 0;
        auto A = [&] { return r.eng.display.src[(int) Src::plateA].load(); };
        const int pad = 48, ch = 10;
        r.render (4800, { on (0, pad, 100, ch), pat (1000, pad, 110, ch), off (2400, pad, ch) });   // single tap
        r.render (24000);
        CHECK (A() < 0.05f, "single tap: plate A drops back (%.2f)", A());
        r.render (9600, { on (0, pad, 100, ch), off (2400, pad, ch), on (6000, pad, 100, ch), pat (6500, pad, 120, ch),
                          pat (7400, pad, 20, ch), off (7800, pad, ch) });                      // double tap, lifting off slowly
        r.render (9600);
        CHECK (A() > 0.8f, "double tap locks plate A at its peak (%.2f)", A());
        CHECK (r.eng.display.plateLocked[12].load(), "lock shows on the display");
        r.render (9600, { on (0, pad, 100, ch), off (2400, pad, ch), on (6000, pad, 100, ch), off (7000, pad, ch) });
        r.render (9600);
        CHECK (A() < 0.05f && ! r.eng.display.plateLocked[12].load(), "double tap again unlocks (%.2f)", A());
    }

    // 12. drums: every groove plays, sane levels; a four-on-the-floor at 120 hits 8 kicks in 4 s
    {
        for (auto& g : grooves())
        {
            Run r; r.s.reverbMix = 0; applyGroove (r.s.drums, g); r.s.drums.playing = true;
            r.s.drums.tune = { 0.2f, 0.8f, 0.3f, 0.9f, 0.1f, 0.7f, 0.4f, 1.0f };
            r.s.drums.sound = kits()[(size_t) (&g - &grooves()[0]) % kits().size()].sounds;   // try the kits too
            r.render (48000 * 4);
            const float lv = rms (r.L, 0, r.L.size());
            if (std::string (g.name) != "Empty")
                CHECK (finite (r.L) && peak (r.L) <= 1.25f && lv > 0.01f && lv < 0.4f, "groove %-18s rms %.3f peak %.2f", g.name, lv, peak (r.L));
        }
        Run r; r.s.reverbMix = 0; r.s.drums.playing = true; r.s.drums.tempo = 120;
        for (int k = 0; k < 16; k += 4) r.s.drums.steps[0][(size_t) k] = 2;
        r.render (48000 * 4);
        CHECK (r.eng.display.drumKicks.load() == 8, "4/4 kick at 120 bpm: 8 kicks in 4 s (%d)", r.eng.display.drumKicks.load());

        // swing pushes the off-beat 16ths later: hat on every 16th at 120 bpm, swing 1
        Run sw; sw.s.reverbMix = 0; sw.s.drums.playing = true; sw.s.drums.tempo = 120; sw.s.drums.swing = 1.0f;
        sw.s.drums.steps[4] = {}; sw.s.drums.steps[4][1] = 2;   // only the 2nd 16th
        sw.render (12000);
        int first = -1; for (size_t i = 0; i < sw.L.size(); ++i) if (std::abs (sw.L[i]) > 0.01f) { first = (int) i; break; }
        CHECK (first > 6000 + 1700 && first < 6000 + 2300, "swing delays the off-beat (hit at %d, straight would be 6000)", first);
    }

    // 13. looper locked to the drums: recording waits for the bar, loops are whole bars
    {
        Run r; r.s.reverbMix = 0; r.s.drums.playing = true; r.s.drums.tempo = 120;   // a bar = 2 s = 96000 samples
        r.s.drums.steps[0][0] = 2;
        r.render (10000);
        r.eng.looper.requestLoop();                                // pressed mid-bar
        r.render (20000, { on (0, 60) });
        CHECK (r.eng.looper.getState() == Looper::armed, "LOOP waits for the next bar (armed)");
        r.render (80000);                                          // crosses the bar line at 96000
        CHECK (r.eng.looper.getState() == Looper::recording, "recording started on the bar");
        r.render (60000);
        r.eng.looper.requestLoop();                                // pressed before the next bar (192000)
        r.render (512);
        CHECK (r.eng.looper.getState() == Looper::closing, "closing waits for the bar line");
        r.render (40000);
        CHECK (r.eng.looper.getState() == Looper::playing, "loop closed on the bar");
        CHECK (r.eng.looper.length() == 96000, "loop is exactly one bar (%d samples)", r.eng.looper.length());

        // drums stopped: the looper goes back to instant behaviour
        Run f; f.s.drums.playing = false;
        f.eng.looper.requestLoop(); f.render (512);
        CHECK (f.eng.looper.getState() == Looper::recording, "without drums, recording starts at once");
    }

    // 14. pitch bend: MIDI and the on-screen strip, with the bend range
    {
        auto pitchOf = [] (std::vector<float>& x, size_t a, size_t b)   // zero-crossing frequency
        {
            int zc = 0; for (size_t i = a + 1; i < b; ++i) if (x[i - 1] <= 0.0f && x[i] > 0.0f) ++zc;
            return (float) zc * 48000.0f / (float) (b - a);
        };
        Run r; r.s.reverbMix = 0; r.s.aWave = WaveA::sine; r.s.aDetune = 0; r.s.cutoff = 1.0f; r.s.bendRange = 12.0f;
        r.render (24000, { on (0, 57) });                                          // A3 = 220 Hz
        const float f0 = pitchOf (r.L, 4000, 24000);
        r.eng.setUiBend (1.0f); r.L.clear(); r.render (24000);
        const float fUp = pitchOf (r.L, 4000, 24000);
        r.eng.setUiBend (0.0f);
        r.L.clear(); r.render (24000, { { 0, 0xE0, 0, 0 } });                       // MIDI bend fully down
        const float fDown = pitchOf (r.L, 4000, 24000);
        std::printf ("      bend range 12: centre %.0f Hz, strip up %.0f Hz, MIDI down %.0f Hz\n", f0, fUp, fDown);
        CHECK (std::abs (fUp / f0 - 2.0f) < 0.05f && std::abs (fDown / f0 - 0.5f) < 0.03f, "bend reaches an octave each way at range 12");

        r.eng.setUiWheel (0.7f); r.render (512);
        CHECK (std::abs (r.eng.display.src[(int) Src::wheel].load() - 0.7f) < 0.01f, "mod strip sets the wheel");
        r.render (512, { { 0, 0xB0, 1, 0 } });                                     // the keyboard's wheel takes over
        CHECK (r.eng.display.src[(int) Src::wheel].load() < 0.01f, "MIDI mod wheel takes over again");
    }

    // 15. every sound in the library: plays, stays in range, dies away; a captured patch plays from a row
    {
        int bad = 0;
        for (int snd = 0; snd < numSounds; ++snd)
            for (float tune : { 0.0f, 0.5f, 1.0f })
                for (float dec : { 0.0f, 0.5f, 1.0f })
                {
                    Run r; r.s.reverbMix = 0; r.s.drums.playing = true; r.s.drums.tempo = 20;
                    r.s.drums.sound[0] = snd; r.s.drums.tune[0] = tune; r.s.drums.decay[0] = dec; r.s.drums.level[0] = 1.0f;
                    r.s.drums.volume = 1.0f; r.s.drums.steps[0][0] = 2;
                    const int len = 48000 * 12;
                    r.render (len);                                         // one hit (a bar at 20 bpm, the slowest tempo, is 12 s)
                    // (stop short of the end: the next bar's hit lands right on it)
                    const float pk = peak (r.L), tail = rms (r.L, (size_t) (len - 9600), (size_t) (len - 4800));
                    // the longest settings (gong, triangle at full decay) are meant to ring a long time
                    const bool mustDie = dec < 0.9f;
                    if (! finite (r.L) || pk > 1.2f || pk < 0.02f || (mustDie && tail > 0.003f))
                    {
                        ++bad;
                        std::printf ("      %-11s tune %.1f decay %.1f: peak %.3f tail %.4f\n", soundName (snd), tune, dec, pk, tail);
                    }
                }
        CHECK (bad == 0, "all %d sounds x 9 settings: audible, bounded, die away", numSounds);

        DrumSample smp; smp.L.assign (24000, 0.0f); smp.R.assign (24000, 0.0f);
        for (int i = 0; i < 24000; ++i) smp.L[(size_t) i] = smp.R[(size_t) i] = 0.5f * std::sin (twoPi * 220.0f * (float) i / 48000.0f) * std::exp (-(float) i / 8000.0f);
        Run r; r.s.reverbMix = 0; r.s.drums.playing = true; r.s.drums.tempo = 120; r.s.drums.volume = 1.0f;
        r.s.drums.sound[7] = (int) Sound::patch; r.s.drums.level[7] = 1.0f; r.s.drums.steps[7][0] = 2;
        r.eng.drums.setSample (7, &smp);
        r.render (24000);
        CHECK (peak (r.L) > 0.2f && finite (r.L), "a captured patch plays from its row (peak %.2f)", peak (r.L));
        r.s.drums.decay[7] = 0.1f; r.L.clear(); r.eng.drums.setSample (7, &smp);
        r.render (96000);   // next bar
        // the hit lands at 72000 here; at DECAY 0.1 it should be gone ~6000 samples later
        CHECK (rms (r.L, 72000 + 7000, 96000) < 1.0e-3f && rms (r.L, 72000, 72000 + 3000) > 0.05f, "DECAY shortens a patch row");
        r.eng.drums.setSample (7, nullptr);
        r.render (96000);
        CHECK (finite (r.L), "a patch row without a sample stays silent");
    }

    // 16. seamless loops: a held pad wraps with no dip or jump, and overdubs taper instead of chopping
    {
        const float fs = 48000.0f;
        Looper lp; lp.prepare (fs, 20.0f);
        std::mt19937 rng (7);
        std::uniform_real_distribution<float> ph (0.0f, 6.2831853f);
        // a steady organ-like tone on 100 Hz: its level over any 20 ms (two periods) is exactly constant,
        // and both sides of the loop point are the same waveform (the hardest case for a crossfade)
        float freqs[40], phase[40];
        for (int k = 0; k < 40; ++k) { freqs[k] = 100.0f * (float) (k % 8 + 1); phase[k] = ph (rng); }
        long clock = 0;
        auto padSample = [&] (float& l, float& r)
        {
            float v = 0.0f, w = 0.0f;
            for (int k = 0; k < 40; ++k) { const float a = phase[k] + 6.2831853f * freqs[k] * (float) clock / fs; v += std::sin (a); w += std::sin (a + 0.3f * (float) k); }
            l = 0.025f * v; r = 0.025f * w; ++clock;
        };
        std::vector<float> out;
        // run n samples; pad = feed the pad, otherwise silence
        auto run = [&] (int n, bool pad)
        {
            for (int done = 0; done < n; done += 256)
            {
                const int m = std::min (256, n - done);
                float L[256], R[256];
                for (int i = 0; i < m; ++i) { if (pad) padSample (L[i], R[i]); else { L[i] = R[i] = 0.0f; ++clock; } }
                lp.process (L, R, m, 1.0f);
                out.insert (out.end(), L, L + m);
            }
        };
        auto maxStep = [] (const std::vector<float>& x, size_t a, size_t b)
        {
            float m = 0.0f; for (size_t i = a + 1; i < b; ++i) m = std::max (m, std::abs (x[i] - x[i - 1])); return m;
        };
        auto winRange = [] (const std::vector<float>& x, size_t a, size_t b, int w, float& lo, float& hi)
        {
            lo = 1.0e9f; hi = -1.0e9f;
            for (size_t s0 = a; s0 + (size_t) w <= b; s0 += (size_t) w / 2)
            {
                double e = 0.0; for (int i = 0; i < w; ++i) e += x[s0 + (size_t) i] * x[s0 + (size_t) i];
                const float db = 10.0f * std::log10 ((float) (e / w) + 1.0e-12f);
                lo = std::min (lo, db); hi = std::max (hi, db);
            }
        };

        run (48000, true);                    // the chord is already sounding
        float padLo, padHi; winRange (out, 0, out.size(), 960, padLo, padHi);
        out.clear();
        lp.requestLoop(); run (110000, true); // record ~2.3 s
        lp.requestLoop();                     // close while still holding
        out.clear();
        run (256, false);
        const int len = lp.length();
        out.clear();
        run (len * 3, false);                 // let go: just the loop, three times round
        const float padStep = maxStep (out, 1000, (size_t) len - 30000);
        float lo, hi; winRange (out, 4800, out.size(), 960, lo, hi);   // (after the playback fade-in settles)
        CHECK (hi - lo < (padHi - padLo) + 1.5f, "held pad loops with no dip at the loop point (20 ms level range %.2f dB, the pad itself %.2f dB)", hi - lo, padHi - padLo);
        const float wrapStep = std::max (maxStep (out, (size_t) len - 2400, (size_t) len + 2400), maxStep (out, (size_t) (2 * len) - 2400, (size_t) (2 * len) + 2400));
        CHECK (wrapStep < padStep * 1.5f, "no jump at the wrap (step %.4f, pad's own %.4f)", wrapStep, padStep);

        // and a sound that's different on each side of the join (a noisy wash): the level holds too
        {
            Looper ln; ln.prepare (fs, 20.0f);
            std::normal_distribution<float> nd (0.0f, 0.1f);
            std::vector<float> o2;
            auto run2 = [&] (int n, bool on)
            {
                for (int done = 0; done < n; done += 256)
                {
                    const int m = std::min (256, n - done);
                    float L[256], R[256];
                    for (int i = 0; i < m; ++i) { L[i] = on ? nd (rng) : 0.0f; R[i] = on ? nd (rng) : 0.0f; }
                    ln.process (L, R, m, 1.0f);
                    o2.insert (o2.end(), L, L + m);
                }
            };
            run2 (48000, true);
            float nLo, nHi; winRange (o2, 0, o2.size(), 960, nLo, nHi);
            ln.requestLoop(); run2 (100000, true); ln.requestLoop(); run2 (256, false);
            o2.clear(); run2 (ln.length() * 2, false);
            float l2, h2; winRange (o2, 4800, o2.size(), 960, l2, h2);
            CHECK (h2 - l2 < (nHi - nLo) + 1.0f, "a noisy wash loops with a steady level too (range %.2f dB, the sound itself %.2f dB)", h2 - l2, nHi - nLo);
        }

        // overdub: a fresh, silent 2 s loop, then a chord punched in and out while it keeps sounding
        lp.requestClear(); run (256, false);
        lp.requestLoop(); run (96000, false); lp.requestLoop(); run (256, false);
        const int len2 = lp.length();
        while (true) { out.clear(); run (256, false); if (lp.length() == len2 && out.size() && std::abs (out[0]) < 1.0f) break; }
        lp.requestLoop(); run (48000, true);   // overdub 1 s of chord
        lp.requestLoop(); run (48000, true);   // punch out but keep holding
        run (len2, false);
        out.clear(); run (len2 * 2, false);     // two passes of the loop alone
        const float dubStep = maxStep (out, 0, out.size());
        winRange (out, 0, out.size(), 960, lo, hi);
        CHECK (dubStep < padStep * 1.5f && hi > -30.0f, "overdub punch in/out has no chop (max step %.4f, pad's own %.4f)", dubStep, padStep);
        lp.requestUndo(); out.clear(); run (len2, false);
        CHECK (peak (out) < 1.0e-6f, "UNDO takes the overdub back, taper included (peak %.6f)", peak (out));
    }

    // 17. STRING mode: in tune, decays, brighter with SHAPE, buzzes at the top, stays bounded
    {
        auto pluck = [] (int note, float shape, int vel, int len, float detune = 0.0f)
        {
            Run r; auto& s = r.s;
            s.aWave = WaveA::string; s.aShape = shape; s.aDetune = detune; s.aLevel = 0.8f; s.bLevel = 0.0f; s.air = 0.0f;
            s.cutoff = 1.0f; s.res = 0.0f; s.drive = 0.0f; s.envAmt = 0.5f; s.reverbMix = 0.0f; s.chorusMix = 0.0f; s.delayMix = 0.0f;
            s.ampA = 0.0f; s.ampS = 1.0f; s.ampR = 0.7f;
            r.render (len, { on (0, note, vel) });
            return r.L;
        };
        // pitch by autocorrelation over 100 ms (search 0.5x..2x the expected period)
        auto measureHz = [] (const std::vector<float>& x, size_t from, float expectHz)
        {
            const int n = 4800; const float fs = 48000.0f;
            const int lo = (int) (fs / (expectHz * 1.4f)), hi = (int) (fs / (expectHz * 0.7f));
            int best = lo; double bestC = -1e9;
            for (int lag = lo; lag <= hi; ++lag)
            {
                double c = 0, e = 1e-12, e0 = 1e-12;
                for (int i = 0; i < n; ++i) { c += x[from + i] * x[from + i + lag]; e += x[from + i + lag] * x[from + i + lag]; e0 += x[from + i] * x[from + i]; }
                c /= std::sqrt (e * e0);
                if (c > bestC) { bestC = c; best = lag; }
            }
            // parabolic refinement
            auto corr = [&] (int lag) { double c = 0, e = 1e-12, e0 = 1e-12; for (int i = 0; i < n; ++i) { c += x[from + i] * x[from + i + lag]; e += x[from + i + lag] * x[from + i + lag]; e0 += x[from + i] * x[from + i]; } return c / std::sqrt (e * e0); };
            const double a = corr (best - 1), b = corr (best), c = corr (best + 1);
            const double off = 0.5 * (a - c) / (a - 2 * b + c);
            return fs / (float) (best + off);
        };
        float worstCents = 0.0f;
        for (int note : { 31, 55, 57, 69, 81, 93 })   // (36..51 play the plates)
        {
            const auto x = pluck (note, 0.3f, 100, 24000);
            const float want = 440.0f * std::pow (2.0f, (float) (note - 69) / 12.0f);
            const float got = measureHz (x, 4800, want);
            worstCents = std::max (worstCents, std::abs (1200.0f * std::log2 (got / want)));
            std::printf ("      note %d: want %.2f Hz, got %.2f Hz\n", note, want, got);
        }
        CHECK (worstCents < 8.0f, "STRING is in tune (worst %.1f cents over G1..A6)", worstCents);

        auto hfRatio = [] (const std::vector<float>& x, size_t a, size_t b)
        {
            double d = 0, e = 1e-12; for (size_t i = a + 1; i < b; ++i) { d += (x[i] - x[i - 1]) * (x[i] - x[i - 1]); e += x[i] * x[i]; } return (float) std::sqrt (d / e);
        };
        const auto nylon = pluck (55, 0.05f, 100, 48000), steel = pluck (55, 0.6f, 100, 48000), sitar = pluck (55, 1.0f, 100, 48000);
        const float hN = hfRatio (nylon, 2400, 24000), hS = hfRatio (steel, 2400, 24000), hB = hfRatio (sitar, 36000, 48000), hS2 = hfRatio (steel, 36000, 48000);
        CHECK (hS > hN * 1.5f, "SHAPE brightens it (nylon %.3f -> steel %.3f)", hN, hS);
        CHECK (hB > hS2 * 1.3f, "the top of SHAPE buzzes and keeps buzzing (steel %.3f -> sitar %.3f, 0.75-1 s in)", hS2, hB);

        const auto ring = pluck (33, 0.4f, 100, 48000 * 4);
        const float early = rms (ring, 4800, 19200), late = rms (ring, 48000 * 3, 48000 * 4);
        CHECK (late < early * 0.5f && late > early * 0.005f, "a string rings on and dies away (%.3f -> %.4f over 3 s)", early, late);

        float pk = 0.0f; bool ok = true;
        for (float sh : { 0.0f, 0.5f, 0.8f, 1.0f })
            for (int vel : { 20, 127 })
                for (int note : { 28, 60, 100 })
                {
                    const auto x = pluck (note, sh, vel, 24000, 0.6f);
                    pk = std::max (pk, peak (x)); ok = ok && finite (x);
                }
        CHECK (ok && pk < 1.25f && pk > 0.05f, "every SHAPE / velocity / register is bounded (peak %.2f)", pk);
    }

    // 18. MPE: each member channel bends, presses and slides only its own note
    {
        auto bendMsg = [] (int at, int ch, float b) { const int v = std::min (16383, std::max (0, (int) std::lround (8192.0f + b * 8192.0f)));
                                                      return MidiEvent { at, (uint8_t) (0xE0 | (ch - 1)), (uint8_t) (v & 127), (uint8_t) (v >> 7) }; };
        auto pressMsg = [] (int at, int ch, int v) { return MidiEvent { at, (uint8_t) (0xD0 | (ch - 1)), (uint8_t) v, 0 }; };
        auto ccMsg = [] (int at, int ch, int cc, int v) { return MidiEvent { at, (uint8_t) (0xB0 | (ch - 1)), (uint8_t) cc, (uint8_t) v }; };
        auto voiceOn = [] (Run& r, int ch, int note) -> const Voice*
        {
            return r.eng.findVoice ((ch << 8) | note);
        };
        Run r; r.s.mpe = 48; r.s.aWave = WaveA::sine; r.s.aShape = 0; r.s.bLevel = 0; r.s.reverbMix = 0; r.s.chorusMix = 0; r.s.delayMix = 0;
        r.render (2400, { bendMsg (0, 2, 0.25f), pressMsg (0, 2, 30), ccMsg (0, 2, 74, 90), on (10, 57, 100, 2),
                          on (20, 40, 100, 3) });   // note 40 on a member channel is a note, not a plate
        const Voice* a = voiceOn (r, 2, 57); const Voice* b = voiceOn (r, 3, 40);
        CHECK (a != nullptr && b != nullptr, "MPE notes on channels 2 and 3 both play (40 is not taken as a pad)");
        if (a && b)
        {
            CHECK (std::abs (a->noteBend - 12.0f) < 0.1f && std::abs (b->noteBend) < 0.01f,
                   "bend sent before a note applies to that note only (%.2f, %.2f semitones)", a->noteBend, b->noteBend);
            r.render (4800, { bendMsg (0, 3, -0.5f), pressMsg (0, 3, 120), ccMsg (0, 3, 74, 10) });
            CHECK (std::abs (b->noteBend + 24.0f) < 0.2f && std::abs (a->noteBend - 12.0f) < 0.1f, "per-note bend (%.2f / %.2f)", a->noteBend, b->noteBend);
            CHECK (b->press > 0.85f && a->press < 0.3f, "per-note pressure (%.2f / %.2f)", a->press, b->press);
            CHECK (a->slide > 0.65f && b->slide < 0.15f, "per-note slide (%.2f / %.2f)", a->slide, b->slide);
        }
        // pitch of the bent note: 220 Hz bent up 12 = 440 Hz (count zero crossings of that note alone)
        {
            Run q; q.s.mpe = 48; q.s.aWave = WaveA::sine; q.s.aShape = 0; q.s.bLevel = 0; q.s.air = 0; q.s.reverbMix = 0; q.s.chorusMix = 0; q.s.delayMix = 0; q.s.cutoff = 1.0f;
            q.s.aDetune = 0.0f;
            q.render (48000, { bendMsg (0, 5, 0.25f), on (0, 57, 100, 5) });
            int zc = 0; for (size_t i = 24001; i < 48000; ++i) if (q.L[i - 1] < 0.0f && q.L[i] >= 0.0f) ++zc;
            CHECK (std::abs (zc * 2 - 440) <= 4, "a bent MPE note sounds at the bent pitch (%d Hz, want 440)", zc * 2);
        }
        // MPE off: channel bend is global again, and pads are pads
        Run g; g.s.mpe = 0;
        g.render (2400, { on (0, 57, 100, 2), on (0, 60, 100, 3), bendMsg (100, 2, 0.5f), on (200, 40, 100, 4) });
        CHECK (std::abs (g.eng.globalBendSemis() - 1.0f) < 0.01f && voiceOn (g, 4, 40) == nullptr, "MPE off: bend is global and pad notes play plates");
    }

    std::printf (failures == 0 ? "\nALL PASSED\n" : "\n%d FAILED\n", failures);
    return failures == 0 ? 0 : 1;
}
