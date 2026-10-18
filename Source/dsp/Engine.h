#pragma once

// The whole Zenthesizer sound engine: MIDI in -> plates / keys / arp -> 8 voices -> FX -> stereo out.
// Plain C++ (no JUCE) so it can be tested on its own.

#include "Common.h"
#include "Fx.h"
#include "Looper.h"
#include "Maps.h"
#include "Settings.h"
#include "Voice.h"
#include "Wavetables.h"
#include <array>
#include <atomic>
#include <cstdint>

namespace zen
{
    struct MidiEvent
    {
        int offset = 0;
        uint8_t status = 0, d1 = 0, d2 = 0;
    };

    // Written by the audio thread, read by the UI's timer. Relaxed atomics only.
    struct Display
    {
        static constexpr int scopeSize = 1024, maxVoices = 8;
        std::array<std::atomic<float>, scopeSize> scope {};
        std::atomic<int> scopeWrite { 0 };

        std::array<std::atomic<float>, maxVoices> voiceLevel {}, voicePitch {}, voicePress {};
        std::array<std::atomic<int>, maxVoices> voicePlate {};
        std::array<std::atomic<float>, numPlates> plateLevel {};   // note plates: glow; timbre plates: value
        std::array<std::atomic<bool>, numPlates> plateLocked {};
        std::array<std::atomic<float>, numSrc> src {};
        std::array<std::atomic<float>, numDst> dstNorm {};
        std::atomic<int> noteOnCount { 0 }, lastPlate { -1 };
        std::atomic<float> lastPitch { 60.0f }, lastVel { 0.0f };
        std::atomic<int> inCount { 0 }, inNote { -1 }, inChannel { 0 };   // last incoming note-on (for pad learn)
        std::atomic<float> cutoffHzNow { 1000.0f };
        std::atomic<int> drumStep { -1 }, drumKicks { 0 }, drumHits { 0 };
        std::atomic<float> bendNow { 0.0f };   // -1..1, MIDI + screen
        std::atomic<int> activeVoices { 0 };
    };

    class Engine
    {
    public:
        static constexpr int maxVoices = Display::maxVoices;
        static constexpr int chunk = 32;

        Engine()
        {
            for (auto& u : uiPlate) u.store (-1.0f);
            for (auto& u : uiChord) u.store (-1.0f);
            uiLast.fill (-1.0f); uiChordLast.fill (-1.0f);
            (void) Wavetables::get();   // build the tables up front, not on the audio thread
        }

        // lightweight: a throwaway engine used to capture a patch (no room for long loops)
        void prepare (double sampleRate, bool lightweight = false)
        {
            fs = (float) sampleRate;
            chorus.prepare (fs); delay.prepare (fs); reverb.prepare (fs); limiter.prepare (fs);
            looper.prepare (fs, lightweight ? 0.5f : 45.0f);
            drums.prepare (fs);
            drumL.assign (4096, 0.0f); drumR.assign (4096, 0.0f);
            for (auto& v : voices) { v.active = false; v.amp.reset(); v.noiseHP.setCutoff (300.0f, fs); v.noiseLP.setCutoff (7000.0f, fs); }
            bufL.assign (4096, 0.0f); bufR.assign (4096, 0.0f);
            drift1.prepare (fs / (float) chunk, 1.0f, 11u);
            drift2.prepare (fs / (float) chunk, 1.0f, 23u);
            reset();
        }

        void reset()
        {
            for (auto& v : voices) { v.active = false; v.gate = false; v.amp.reset(); v.e1.reset(); v.e2.reset(); }
            chorus.clear(); delay.clear(); reverb.clear();
            for (auto& p : plates) p = PlateState {};
            for (auto& c : chordNotes) c = ChordNotes {};
            numPending = 0;
            heldCount = 0; arpPlaying = -1; arpCounter = 0.0; arpStep = 0;
            sustainPedal = false;
        }

        // The user's own table (Engine B, table 4). The pointer must stay valid for a while after
        // it's replaced: the owner retires old tables with a grace period, never immediately.
        void setUserTable (const UserTable* t) noexcept { userTablePtr.store (t); }

        // UI: the on-screen pitch strip (-1..1, springs back) and mod strip (0..1, stays put)
        void setUiBend (float b) noexcept  { uiBend.store (juce_clampBend (b)); }
        void setUiWheel (float w) noexcept { uiWheel.store (clamp01 (w)); uiWheelSeq.fetch_add (1); }

        // for tests and the UI: a sounding voice by its key id ((channel << 8) | note), and the global bend
        const Voice* findVoice (int id) const noexcept { for (auto& v : voices) if (v.active && v.id == id) return &v; return nullptr; }
        float globalBendSemis() const noexcept { return bendSemis(); }

        // UI: press/release a plate with the mouse (pressure 0..1, or < 0 for released).
        void setUiPlate (int plate, float pressure) noexcept
        {
            if (plate >= 0 && plate < numPlates) uiPlate[(size_t) plate].store (pressure);
        }
        // UI: press/release a chord button (degree 0..6), same convention.
        void setUiChord (int degree, float pressure) noexcept
        {
            if (degree >= 0 && degree < numChords) uiChord[(size_t) degree].store (pressure);
        }

        void process (const Settings& s, const MidiEvent* ev, int numEv, float* outL, float* outR, int n)
        {
            set = &s;
            userNow = userTablePtr.load();
            if (const int seq = uiWheelSeq.load(); seq != uiWheelSeen) { uiWheelSeen = seq; wheel = uiWheel.load(); }   // last touched wins
            pollUiPlates();
            pollUiChords();
            int e = 0;
            for (int start = 0; start < n; start += chunk)
            {
                const int len = std::min (chunk, n - start);
                while (e < numEv && ev[e].offset < start + len) handleMidi (ev[e++]);
                controlTick (len);
                std::fill (outL + start, outL + start + len, 0.0f);
                std::fill (outR + start, outR + start + len, 0.0f);
                for (int vi = 0; vi < maxVoices; ++vi)
                    if (voices[(size_t) vi].active) renderVoice (vi, outL + start, outR + start, len);
            }
            while (e < numEv) handleMidi (ev[e++]);

            // FX in blocks of up to 4096
            for (int start = 0; start < n; start += 4096)
            {
                const int len = std::min (4096, n - start);
                float* L = outL + start; float* R = outR + start;
                chorus.process (L, R, len, clamp01 (s.chorusMix + gMod[(int) Dst::chorusMix]), s.chorusDepth, s.chorusRate);
                delay.process (L, R, len, s.delayTime, s.delaySync, tempo(), s.delayFeedback, s.delayTone,
                               clamp01 (s.delayMix + gMod[(int) Dst::delayMix]));
                reverb.process (L, R, len, s.reverbSize, s.reverbDecay, s.reverbDamp, s.reverbShimmer,
                                clamp01 (s.reverbMix + gMod[(int) Dst::reverbMix]));
                const float vol = dbToGain (volumeDb (s.volume));
                for (int i = 0; i < len; ++i) { L[i] *= vol; R[i] *= vol; }   // no clipping here: the limiter at the end catches peaks
                // the drums: own clock (or the host's), bar lines for the looper
                int bars[DrumMachine::maxBars]; int numBars = 0;
                const float drumBpm = s.bpm > 1.0f ? s.bpm : s.drums.tempo;
                const double ppq = s.hostPpq >= 0.0 ? s.hostPpq + (double) start * drumBpm / 60.0 / fs : -1.0;
                drums.process (s.drums, drumBpm, ppq, drumL.data(), drumR.data(), len, bars, numBars);

                // the looper records what you hear and plays it back on top, untouched by preset changes
                looper.process (L, R, len, s.loopLevel, bars, numBars, s.drums.playing);
                for (int i = 0; i < len; ++i) { L[i] += drumL[(size_t) i] * voiceHeadroom; R[i] += drumR[(size_t) i] * voiceHeadroom; }   // same headroom as the synth, so the balance is unchanged
                limiter.process (L, R, len);   // a clean peak limiter instead of a clipper: chords and loops stay undistorted
                for (int i = 0; i < len; ++i)
                {
                    const int w = scopeW;
                    display.scope[(size_t) w].store (0.5f * (L[i] + R[i]), std::memory_order_relaxed);
                    scopeW = (w + 1) & (Display::scopeSize - 1);
                }
            }
            display.scopeWrite.store (scopeW, std::memory_order_relaxed);
            sampleClock += (uint64_t) n;
            publishDisplay();
        }

        // Each voice sits 6 dB below full scale, so chords and pads have room before the limiter.
        static constexpr float voiceHeadroom = 0.5f;

        Display display;
        Limiter limiter;
        Looper looper;
        DrumMachine drums;

    private:
        //======================================================================
        struct PlateState
        {
            bool down = false, fromUi = false;
            bool locked = false;              // timbre plates: double-tap holds the value
            float press = 0.0f, target = 0.0f, peak = 0.0f;
            uint64_t lastDown = 0, lastUp = 0;
        };
        uint64_t sampleClock = 0;

        void setPlatePressure (int p, float v)
        {
            auto& ps = plates[(size_t) p];
            if (p >= timbrePlate0 && ps.locked && ! ps.down) return;   // locked and let go: keep it
            ps.target = v;
            if (ps.down) ps.peak = std::max (ps.peak, v);
        }

        struct Held { int id; float pitch; float vel; };

        const Settings* set = nullptr;
        std::atomic<const UserTable*> userTablePtr { nullptr };
        const UserTable* userNow = nullptr;
        float fs = 48000.0f;
        std::array<Voice, maxVoices> voices {};
        std::array<PlateState, numPlates> plates {};
        std::array<std::atomic<float>, numPlates> uiPlate {};
        std::array<float, numPlates> uiLast {};
        std::array<std::atomic<float>, numChords> uiChord {};
        std::array<float, numChords> uiChordLast {};

        // chords: which notes each chord button started, and strummed notes still waiting to start
        struct ChordNotes { int n = 0; int ids[6] {}; };
        std::array<ChordNotes, numChords> chordNotes {};
        struct Pending { int id; float pitch; float vel; double wait; int chord; };
        std::array<Pending, 48> pending {};
        int numPending = 0;
        static int chordId (int degree, int k) noexcept { return 20000 + degree * 16 + k; }
        uint32_t serial = 1;

        float wheel = 0.0f, chanAT = 0.0f, bend = 0.0f;   // bend: -1..1 from MIDI
        // MPE: each member channel (2..16) carries one note's bend, pressure and slide
        std::array<float, 17> chBend {}, chPress {}, chSlide {};
        float slideAll = 0.0f;   // CC74 from a non-MPE controller moves every note's slide
        bool isMember (int ch) const noexcept { return set->mpe > 0 && ch >= 2; }
        static int channelOf (int id) noexcept { return id < 10000 ? (id >> 8) : 0; }
        template <typename Fn> void forChannel (int ch, Fn fn) { for (auto& v : voices) if (v.active && channelOf (v.id) == ch) fn (v); }
        std::atomic<float> uiBend { 0.0f };               // -1..1 from the on-screen strip
        std::atomic<float> uiWheel { 0.0f };
        std::atomic<int> uiWheelSeq { 0 };
        int uiWheelSeen = 0;
        static float juce_clampBend (float b) noexcept { return std::min (1.0f, std::max (-1.0f, b)); }
        float bendSemis() const noexcept { return juce_clampBend (bend + uiBend.load (std::memory_order_relaxed)) * set->bendRange; }
        bool sustainPedal = false;

        // modulators
        float lfo1Phase = 0.0f, lfo2Phase = 0.0f, lfo1 = 0.0f, lfo2 = 0.0f;
        Wander drift1, drift2;
        float randHeld = 0.0f, randOut = 0.0f, randPhase = 0.0f;
        Rng rng;
        std::array<float, numDst> gMod {};

        // arpeggiator
        std::array<Held, 32> held {};
        int heldCount = 0;
        bool arpLatched = false;   // hold mode: keys released, pattern still playing
        int arpPlaying = -1;       // voice id currently sounding
        double arpCounter = 0.0, arpGateLeft = 0.0;
        int arpStep = 0;

        Chorus chorus; PingPong delay; Reverb reverb;
        std::vector<float> bufL, bufR, drumL, drumR;
        int scopeW = 0;

        //======================================================================
        // One tempo for everything: the host's when there is one, otherwise the BEAT page's TEMPO
        // (the Android and standalone apps), so drums, arp, synced echoes and the looper agree.
        float tempo() const noexcept { return set->bpm > 1.0f ? set->bpm : std::max (20.0f, set->drums.tempo); }

        static int keyId (int ch, int note) noexcept   { return (ch << 8) | note; }
        static int plateId (int p) noexcept            { return 10000 + p; }

        int plateForNote (int note, int channel) const noexcept
        {
            if (! set->platesFromPads) return -1;
            if (set->padChannel != 0 && set->padChannel != channel) return -1;
            if (isMember (channel) && set->padChannel != channel) return -1;   // MPE notes are notes, not pads
            for (int p = 0; p < numPlates; ++p)
                if (set->padNotes[(size_t) p] == note) return p;
            return -1;
        }

        void handleMidi (const MidiEvent& m)
        {
            const int type = m.status & 0xF0, ch = (m.status & 0x0F) + 1;
            if (type == 0x90 && m.d2 > 0)
            {
                display.inNote.store (m.d1); display.inChannel.store (ch);
                display.inCount.fetch_add (1);
                const int p = plateForNote (m.d1, ch);
                if (p >= 0) plateDown (p, m.d2 / 127.0f, false);
                else
                {
                    noteOn (keyId (ch, m.d1), (float) m.d1, m.d2 / 127.0f, -1);
                    // the expression that arrived on this note's channel just before it
                    const int id = keyId (ch, m.d1);
                    for (auto& v : voices)
                        if (v.active && v.id == id)
                        {
                            if (isMember (ch))
                            {
                                v.noteBend = v.noteBendTarget = chBend[(size_t) ch] * (float) set->mpe;
                                v.pressTarget = chPress[(size_t) ch];
                                v.slide = v.slideTarget = chSlide[(size_t) ch];
                            }
                            else v.slideTarget = slideAll;
                        }
                }
            }
            else if (type == 0x80 || (type == 0x90 && m.d2 == 0))
            {
                const int p = plateForNote (m.d1, ch);
                if (p >= 0) plateUp (p);
                else noteOff (keyId (ch, m.d1));
            }
            else if (type == 0xA0)   // polyphonic aftertouch
            {
                const float v = m.d2 / 127.0f;
                const int p = plateForNote (m.d1, ch);
                if (p >= 0) setPlatePressure (p, v);
                else setPressure (keyId (ch, m.d1), v);
            }
            else if (type == 0xD0)
            {
                if (isMember (ch)) { chPress[(size_t) ch] = m.d1 / 127.0f; forChannel (ch, [&] (Voice& v) { v.pressTarget = m.d1 / 127.0f; }); }
                else chanAT = m.d1 / 127.0f;
            }
            else if (type == 0xE0)
            {
                const float b = juce_clampBend (((float) ((m.d2 << 7) | m.d1) - 8192.0f) / 8192.0f);
                if (isMember (ch)) { chBend[(size_t) ch] = b; forChannel (ch, [&] (Voice& v) { v.noteBendTarget = b * (float) set->mpe; }); }
                else bend = b;
            }
            else if (type == 0xB0)
            {
                if (m.d1 == 1) wheel = m.d2 / 127.0f;
                else if (m.d1 == 74)
                {
                    const float sv = m.d2 / 127.0f;
                    if (isMember (ch)) { chSlide[(size_t) ch] = sv; forChannel (ch, [&] (Voice& v) { v.slideTarget = sv; }); }
                    else { slideAll = sv; for (auto& v : voices) v.slideTarget = sv; }
                }
                else if (m.d1 == 64)
                {
                    const bool on = m.d2 >= 64;
                    if (sustainPedal && ! on)
                        for (auto& v : voices) if (v.active && v.sustained) { v.sustained = false; v.release(); }
                    sustainPedal = on;
                }
                else if (m.d1 == 123 || m.d1 == 120)
                {
                    for (auto& v : voices) if (v.active) v.release();
                    heldCount = 0; arpLatched = false;
                }
            }
        }

        // ---- plates -----------------------------------------------------------
        void plateDown (int p, float vel, bool fromUi)
        {
            auto& ps = plates[(size_t) p];
            ps.down = true; ps.fromUi = fromUi;
            if (p < numNotePlates)
            {
                ps.target = fromUi ? vel : 0.0f;
                int note = keyNote (p, set->octave);
                if (! inScale (note, set->root, set->scale))
                {
                    if (set->keyScale == KeyScale::mute) return;                        // muted: out-of-scale keys stay silent
                    if (set->keyScale == KeyScale::snap) note = snapToScale (note, set->root, set->scale);
                }
                noteOn (plateId (p), (float) note, fromUi ? 0.8f : vel, p);
            }
            else
            {
                // double-tap (two quick taps) locks or unlocks a timbre plate
                const uint64_t now = sampleClock;
                const auto q = (uint64_t) (0.4f * fs);
                const bool quickFirstTap = ps.lastUp > ps.lastDown && ps.lastUp - ps.lastDown < q;
                if (ps.lastDown != 0 && now - ps.lastDown < q && quickFirstTap)
                {
                    ps.locked = ! ps.locked;
                    ps.lastDown = 0;                 // a third tap starts a new pair
                }
                else ps.lastDown = now == 0 ? 1 : now;

                // the strike itself sets a starting value, pressure takes over
                const bool keep = set->latchTimbre || ps.locked;
                ps.target = std::max (ps.target * (keep ? 1.0f : 0.0f), vel * 0.6f);
                ps.peak = fromUi ? vel : vel * 0.6f;
            }
        }

        void plateUp (int p)
        {
            auto& ps = plates[(size_t) p];
            ps.down = false;
            if (p < numNotePlates) { ps.target = 0.0f; noteOff (plateId (p)); }
            else
            {
                ps.lastUp = sampleClock == 0 ? 1 : sampleClock;
                if (ps.locked) ps.target = ps.peak;          // hold the hardest point of that press
                else if (! set->latchTimbre) ps.target = 0.0f;
            }
        }

        // ---- chords ---------------------------------------------------------------
        void chordDown (int degree, float vel)
        {
            const auto& s = *set;
            const auto c = buildChord (degree, s.root, s.scale, s.octave, s.chord7, s.chord9, s.chordSus, s.chordSpread);
            auto& cn = chordNotes[(size_t) degree];
            chordUp (degree);
            // strum: each note a little after the last, low to high (up to ~120 ms apart)
            const double gap = s.chordStrum ? (0.012 + 0.11 * (1.0 - (double) clamp01 (s.strumSpeed))) * fs : 0.0;
            for (int k = 0; k < c.n; ++k)
            {
                const int id = chordId (degree, k);
                cn.ids[cn.n++] = id;
                if (k == 0 || gap <= 0.0) noteOn (id, (float) c.notes[k], vel, -1);
                else if (numPending < (int) pending.size()) pending[(size_t) numPending++] = { id, (float) c.notes[k], vel * 0.95f, gap * k, degree };
            }
        }

        void chordUp (int degree)
        {
            auto& cn = chordNotes[(size_t) degree];
            for (int k = 0; k < cn.n; ++k) noteOff (cn.ids[k]);
            cn.n = 0;
            int w = 0;   // drop any of its strummed notes that haven't started yet
            for (int i = 0; i < numPending; ++i) if (pending[(size_t) i].chord != degree) pending[(size_t) w++] = pending[(size_t) i];
            numPending = w;
        }

        void tickPending (int len)
        {
            int w = 0;
            for (int i = 0; i < numPending; ++i)
            {
                auto p = pending[(size_t) i];
                p.wait -= len;
                if (p.wait <= 0.0) noteOn (p.id, p.pitch, p.vel, -1);
                else pending[(size_t) w++] = p;
            }
            numPending = w;
        }

        void pollUiChords()
        {
            for (int d = 0; d < numChords; ++d)
            {
                const float u = uiChord[(size_t) d].load();
                const float last = uiChordLast[(size_t) d];
                if (u >= 0.0f && last < 0.0f) chordDown (d, 0.8f);
                else if (u < 0.0f && last >= 0.0f) chordUp (d);
                else if (u >= 0.0f)
                    for (int k = 0; k < chordNotes[(size_t) d].n; ++k) setPressure (chordNotes[(size_t) d].ids[k], u);
                uiChordLast[(size_t) d] = u;
            }
        }

        void pollUiPlates()
        {
            for (int p = 0; p < numPlates; ++p)
            {
                const float u = uiPlate[(size_t) p].load();
                const float last = uiLast[(size_t) p];
                if (u >= 0.0f && last < 0.0f) plateDown (p, std::max (0.05f, u), true);
                else if (u < 0.0f && last >= 0.0f) plateUp (p);
                else if (u >= 0.0f) setPlatePressure (p, u);
                uiLast[(size_t) p] = u;
            }
        }

        // ---- notes ------------------------------------------------------------
        void noteOn (int id, float pitch, float vel, int plate)
        {
            if (set->arpOn)
            {
                if (arpLatched) { heldCount = 0; arpLatched = false; }
                if (heldCount < (int) held.size()) held[(size_t) heldCount++] = { id, pitch, vel };
                if (heldCount == 1) { arpCounter = 0.0; arpStep = 0; }
                return;
            }
            startVoice (id, pitch, vel, plate);
        }

        void noteOff (int id)
        {
            if (set->arpOn)
            {
                if (set->arpHold)
                {
                    bool anyDown = false;
                    for (int i = 0; i < heldCount; ++i) if (held[(size_t) i].id != id && isStillDown (held[(size_t) i].id)) anyDown = true;
                    if (! anyDown) arpLatched = true;
                    return;
                }
                for (int i = 0; i < heldCount; ++i)
                    if (held[(size_t) i].id == id)
                    {
                        for (int j = i; j < heldCount - 1; ++j) held[(size_t) j] = held[(size_t) j + 1];
                        --heldCount;
                        break;
                    }
                return;
            }
            for (auto& v : voices)
                if (v.active && v.gate && v.id == id)
                {
                    if (sustainPedal) v.sustained = true;
                    else v.release();
                }
        }

        bool isStillDown (int id) const noexcept
        {
            if (id >= 10000) return plates[(size_t) (id - 10000)].down;
            return false;   // keys: we only see the note-off that is happening now
        }

        void setPressure (int id, float v)
        {
            for (auto& vc : voices) if (vc.active && vc.id == id) vc.pressTarget = v;
        }

        int allocate (int id)
        {
            for (int i = 0; i < maxVoices; ++i) if (voices[(size_t) i].active && voices[(size_t) i].id == id) return i;
            for (int i = 0; i < maxVoices; ++i) if (! voices[(size_t) i].active) return i;
            int best = -1; float bestLevel = 10.0f;
            for (int i = 0; i < maxVoices; ++i)
                if (! voices[(size_t) i].gate && voices[(size_t) i].amp.level < bestLevel) { best = i; bestLevel = voices[(size_t) i].amp.level; }
            if (best >= 0) return best;
            uint32_t oldest = 0xFFFFFFFFu;
            for (int i = 0; i < maxVoices; ++i) if (voices[(size_t) i].age < oldest) { oldest = voices[(size_t) i].age; best = i; }
            return best;
        }

        void startVoice (int id, float pitch, float vel, int plate)
        {
            const int i = allocate (id);
            auto& v = voices[(size_t) i];
            const bool glide = set->glide > 0.001f;
            if (glide && ! v.active) v.pitch = lastStartedPitch;   // glide from the last note played
            const uint32_t seed = serial * 747796405u + 2891336453u;
            v.start (id, pitch, vel, plate, glide, serial++, seed);
            lastStartedPitch = pitch;
            display.noteOnCount.fetch_add (1);
            display.lastPlate.store (plate);
            display.lastPitch.store (pitch);
            display.lastVel.store (vel);
        }
        float lastStartedPitch = 60.0f;

        // ---- arp ----------------------------------------------------------------
        void arpTick (int len)
        {
            if (! set->arpOn)
            {
                if (heldCount > 0 || arpPlaying >= 0) { stopArpVoice(); heldCount = 0; arpLatched = false; }
                return;
            }
            if (arpPlaying >= 0)
            {
                arpGateLeft -= len;
                if (arpGateLeft <= 0.0) stopArpVoice();
            }
            if (heldCount == 0) { arpCounter = 0.0; return; }

            const float bpm = tempo();
            const double stepLen = arpBeats (set->arpRate) * 60.0 / bpm * fs;
            arpCounter -= len;
            if (arpCounter > 0.0) return;
            arpCounter += stepLen;
            if (arpCounter < 0.0) arpCounter = stepLen;

            // build the pattern
            float pitches[128]; float vels[128]; int count = 0;
            int order[32];
            for (int i = 0; i < heldCount; ++i) order[i] = i;
            if (set->arpMode != ArpMode::played)
                std::sort (order, order + heldCount, [this] (int a, int b) { return held[(size_t) a].pitch < held[(size_t) b].pitch; });
            const int oct = std::max (1, std::min (4, set->arpOctaves));
            for (int o = 0; o < oct; ++o)
                for (int i = 0; i < heldCount && count < 128; ++i)
                {
                    pitches[count] = held[(size_t) order[i]].pitch + 12.0f * (float) o;
                    vels[count] = held[(size_t) order[i]].vel;
                    ++count;
                }
            if (count == 0) return;

            int idx = 0;
            switch (set->arpMode)
            {
                case ArpMode::up: case ArpMode::played: idx = arpStep % count; break;
                case ArpMode::down: idx = count - 1 - (arpStep % count); break;
                case ArpMode::upDown:
                {
                    const int cycle = std::max (1, 2 * count - 2);
                    const int k = arpStep % cycle;
                    idx = k < count ? k : cycle - k;
                    break;
                }
                case ArpMode::random: default:
                    idx = (int) ((rng.next() * 0.5f + 0.5f) * (float) count) % count; break;
            }
            ++arpStep;

            stopArpVoice();
            arpPlaying = 20000 + (arpStep & 1023);
            startVoice (arpPlaying, pitches[idx], vels[idx], -1);
            arpGateLeft = stepLen * clampf (set->arpGate, 0.05f, 1.0f);
        }

        void stopArpVoice()
        {
            if (arpPlaying < 0) return;
            for (auto& v : voices) if (v.active && v.gate && v.id == arpPlaying) v.release();
            arpPlaying = -1;
        }

        // ---- control rate -------------------------------------------------------
        static float lfoShape (LfoShape sh, float ph, Wander& dr) noexcept
        {
            switch (sh)
            {
                case LfoShape::sine:     return std::sin (twoPi * ph);
                case LfoShape::triangle: return 1.0f - 4.0f * std::abs (ph - 0.5f);
                case LfoShape::saw:      return 1.0f - 2.0f * ph;
                case LfoShape::square:   return ph < 0.5f ? 1.0f : -1.0f;
                case LfoShape::drift: default: return dr.next();
            }
        }

        float globalSrc (Src sr) const noexcept
        {
            switch (sr)
            {
                case Src::lfo1: return lfo1;
                case Src::lfo2: return lfo2;
                case Src::rand: return randOut;
                case Src::wheel: return wheel;
                case Src::chanAT: return chanAT;
                case Src::plateA: return plates[timbrePlate0].press;
                case Src::plateB: return plates[timbrePlate0 + 1].press;
                case Src::plateC: return plates[timbrePlate0 + 2].press;
                case Src::plateD: return plates[timbrePlate0 + 3].press;
                default: return 0.0f;
            }
        }

        float voiceSrc (Src sr, const Voice& v) const noexcept
        {
            switch (sr)
            {
                case Src::env1:  return v.e1.level;
                case Src::env2:  return v.e2.level;
                case Src::press: return v.press;
                case Src::vel:   return v.vel;
                case Src::key:   return clampf ((v.pitch - 60.0f) / 24.0f, -1.0f, 1.0f);
                case Src::slide: return v.slide;
                default:         return globalSrc (sr);
            }
        }

        const Voice* newestVoice() const noexcept
        {
            const Voice* best = nullptr;
            for (auto& v : voices) if (v.active && (best == nullptr || v.age > best->age)) best = &v;
            return best;
        }

        void controlTick (int len)
        {
            const auto& s = *set;
            if (numPending > 0) tickPending (len);
            arpTick (len);

            // LFOs (rates can be modulated by global sources)
            const float r1 = lfoHz (s.lfo1Rate) * std::pow (2.0f, gMod[(int) Dst::lfo1Rate] * 4.0f);
            const float r2 = lfoHz (s.lfo2Rate) * std::pow (2.0f, gMod[(int) Dst::lfo2Rate] * 4.0f);
            lfo1Phase += r1 * (float) len / fs; lfo1Phase -= std::floor (lfo1Phase);
            lfo2Phase += r2 * (float) len / fs; lfo2Phase -= std::floor (lfo2Phase);
            drift1.period = std::max (1, (int) (fs / (float) len / std::max (0.02f, r1)));
            drift2.period = std::max (1, (int) (fs / (float) len / std::max (0.02f, r2)));
            lfo1 = lfoShape (s.lfo1Shape, lfo1Phase, drift1);
            lfo2 = lfoShape (s.lfo2Shape, lfo2Phase, drift2);

            // smoothed random
            randPhase += lfoHz (s.randRate) * (float) len / fs;
            if (randPhase >= 1.0f) { randPhase -= std::floor (randPhase); randHeld = rng.next(); }
            const float smoothSec = 0.002f + clamp01 (s.randSmooth) / std::max (0.05f, lfoHz (s.randRate));
            randOut += (randHeld - randOut) * (1.0f - std::exp (-(float) len / (smoothSec * fs)));

            // plates (pressure smoothing ~15 ms)
            const float pc = 1.0f - std::exp (-(float) len / (0.015f * fs));
            for (int p = 0; p < numPlates; ++p) plates[(size_t) p].press += (plates[(size_t) p].target - plates[(size_t) p].press) * pc;
            for (auto& v : voices)
                if (v.active && v.plate >= 0) v.pressTarget = plates[(size_t) v.plate].target;

            // global destinations, from global sources (+ the newest voice for per-voice sources)
            gMod.fill (0.0f);
            const Voice* nv = newestVoice();
            for (auto& sl : s.slots)
            {
                if (sl.src == Src::none || sl.dst == Dst::none || ! isGlobalDst (sl.dst)) continue;
                const float sv = nv != nullptr ? voiceSrc (sl.src, *nv) : globalSrc (sl.src);
                gMod[(size_t) sl.dst] += sl.amount * sv;
            }
        }

        // ---- one voice, one chunk ----------------------------------------------
        void renderVoice (int vi, float* outL, float* outR, int n)
        {
            const auto& s = *set;
            auto& v = voices[(size_t) vi];

            v.amp.set (s.ampA, s.ampD, s.ampS, s.ampR, fs);
            v.e1.set (s.e1A, s.e1D, s.e1S, s.e1R, fs);
            v.e2.set (s.e2A, s.e2D, s.e2S, s.e2R, fs);
            v.e1.advance (n); v.e2.advance (n);
            v.press += (v.pressTarget - v.press) * (1.0f - std::exp (-(float) n / (0.015f * fs)));
            v.slide += (v.slideTarget - v.slide) * (1.0f - std::exp (-(float) n / (0.015f * fs)));
            v.noteBend += (v.noteBendTarget - v.noteBend) * (1.0f - std::exp (-(float) n / (0.003f * fs)));

            float m[numDst] {};
            for (auto& sl : s.slots)
                if (sl.src != Src::none && sl.dst != Dst::none && ! isGlobalDst (sl.dst))
                    m[(int) sl.dst] += sl.amount * voiceSrc (sl.src, v);

            // glide
            const float gs = glideSec (s.glide);
            if (gs <= 0.0f) v.pitch = v.note;
            else v.pitch += (v.note - v.pitch) * (1.0f - std::exp (-(float) n / (gs * 0.4f * fs)));

            const float pitchOffs = m[(int) Dst::pitch] * 12.0f + bendSemis() + v.noteBend;
            const float semisA = v.pitch + pitchOffs + tuneSemisSnapped (s.aTune) + m[(int) Dst::aTune] * 24.0f;
            const float semisB = v.pitch + pitchOffs;
            const float hzA = 440.0f * std::pow (2.0f, (semisA - 69.0f) / 12.0f);
            const float hzB = 440.0f * std::pow (2.0f, (semisB - 69.0f) / 12.0f);

            const float aDet   = clamp01 (s.aDetune + m[(int) Dst::aDetune]);
            const float aShape = clamp01 (s.aShape + m[(int) Dst::aShape]);
            const float aLev   = clamp01 (s.aLevel + m[(int) Dst::aLevel]);
            const float bPos   = clamp01 (s.bPos + m[(int) Dst::bPos]);
            const float bWarp  = clamp01 (s.bWarp + m[(int) Dst::bWarp]);
            const float bUni   = clamp01 (s.bUnison + m[(int) Dst::bUnison]);
            const float bLev   = clamp01 (s.bLevel + m[(int) Dst::bLevel]);
            const float airLev = clamp01 (s.air + m[(int) Dst::air]);
            const float res    = clamp01 (s.res + m[(int) Dst::res]);
            const float drive  = clamp01 (s.drive + m[(int) Dst::drive]);
            const float cutN   = clampf (s.cutoff + m[(int) Dst::cutoff] + bipolar (s.envAmt) * 0.6f * v.e1.level
                                         + s.keyTrack * (v.pitch - 60.0f) / 12.0f * 0.1019f, 0.0f, 1.0f);
            const float ampMod = clampf (1.0f + m[(int) Dst::amp], 0.0f, 2.0f);
            const float velGain = 1.0f - clamp01 (s.velSens) * (1.0f - v.vel);

            // spread voices across the stereo field
            static const float voicePan[maxVoices] = { -0.15f, 0.15f, -0.45f, 0.45f, -0.3f, 0.3f, -0.6f, 0.6f };
            const float pan = clampf (voicePan[vi] * s.spread + m[(int) Dst::pan], -1.0f, 1.0f);
            v.panLT = std::cos ((pan + 1.0f) * 0.25f * pi) * 1.41421f;
            v.panRT = std::sin ((pan + 1.0f) * 0.25f * pi) * 1.41421f;

            v.aGainT = aLev; v.bGainT = bLev; v.airGT = airLev;
            v.cutLogTarget = std::log2 (std::min (cutoffHz (cutN), fs * 0.45f));

            // A: three oscillators (centre + two detuned sides)
            const float cents = std::pow (aDet, 1.5f) * 38.0f;
            const float ratA[3] = { 1.0f, std::pow (2.0f, -cents / 1200.0f), std::pow (2.0f, cents / 1200.0f * 1.07f) };
            const float side = std::min (1.0f, aDet * 5.0f);
            const float aNorm = 1.0f / (1.0f + side * 1.2f);
            float dtA[3];
            for (int k = 0; k < 3; ++k) dtA[k] = std::min (0.45f, hzA * ratA[k] / fs);

            // B: up to five wavetable oscillators
            const float nB = 1.0f + 4.0f * bUni;
            static const float offs[5] = { 0.0f, -0.5f, 0.5f, -1.0f, 1.0f };
            static const float bPanK[5] = { 0.0f, -0.5f, 0.5f, -1.0f, 1.0f };
            float dtB[5], gB[5], bNorm = 0.0f;
            for (int k = 0; k < 5; ++k)
            {
                dtB[k] = std::min (0.45f, hzB * std::pow (2.0f, offs[k] * 25.0f * bUni / 1200.0f) / fs);
                gB[k] = k == 0 ? 1.0f : clamp01 (nB - (float) k) * 0.75f;
                bNorm += gB[k] * gB[k];
            }
            bNorm = 1.0f / std::sqrt (bNorm);
            const float warpK = 0.5f - 0.45f * bWarp;
            const int tableIndex = std::min (userTable, std::max (0, s.bTable));
            const TableView tview = (tableIndex == userTable && userNow != nullptr) ? userNow->view()
                                                                                     : Wavetables::get().view (std::min (numTables - 1, tableIndex));

            // A in STRING mode: a plucked string (and a detuned twin when DETUNE is up)
            const bool isString = s.aWave == WaveA::string;
            float perS[2] {}, gS[2] {}, damp = 0.0f, buzz = 0.0f;
            if (isString)
            {
                const float tone = std::min (1.0f, aShape / 0.66f);
                buzz = clamp01 ((aShape - 0.66f) / 0.34f);
                damp = 0.25f - 0.23f * tone;
                const float t60 = 4.0f + 6.0f * tone + 4.0f * buzz;
                for (int q = 0; q < 2; ++q)
                {
                    const float hz = hzA * ratA[q == 0 ? 0 : 2];
                    perS[q] = fs / std::max (1.0f, hz);
                    gS[q] = std::pow (10.0f, -3.0f / (std::max (1.0f, hz) * t60));
                }
                if (v.pluckPending)
                    for (int q = 0; q < 2; ++q) v.str[q].pluck (perS[q], aShape, v.vel, v.noise);
            }
            v.pluckPending = false;

            const float k = res * 3.95f;
            // Gain staging into the filter: the filter's input saturates (that's what DRIVE is for), so
            // with DRIVE low the signal goes in 6 dB lower and comes back up after, keeping stacked
            // oscillators, unison and chords clean. At full DRIVE it's as hot as it always was.
            const float driveGain = 1.0f + 6.0f * drive;
            const float inTrim = 0.3f + 0.3f * drive;
            const float driveComp = 0.6f / inTrim / (1.0f + 1.6f * drive);
            const float inv = 1.0f / (float) n;

            float G = 0.0f;
            for (int i = 0; i < n; ++i)
            {
                const float t = (float) (i + 1) * inv;
                const float aG = v.aGain + (v.aGainT - v.aGain) * t;
                const float bG = v.bGain + (v.bGainT - v.bGain) * t;
                const float nG = v.airG + (v.airGT - v.airG) * t;

                if ((i & 3) == 0)
                {
                    const float cl = v.cutLog + (v.cutLogTarget - v.cutLog) * t;
                    const float g = std::tan (pi * std::exp2 (cl) / fs);
                    G = g / (1.0f + g);
                }

                // Engine A
                float aL = 0.0f, aR = 0.0f;
                if (isString)
                {
                    const float c0 = v.str[0].process (perS[0], damp, gS[0], buzz);
                    const float c1 = side > 0.001f ? v.str[1].process (perS[1], damp, gS[1], buzz) : 0.0f;
                    aL = (c0 + side * 0.55f * c1) * aNorm * aG * 3.0f;
                    aR = (0.8f * c0 + side * 0.9f * c1) * aNorm * aG * 3.0f;
                }
                else if (aG > 0.0001f)
                {
                    const float c0 = v.a[0].process (s.aWave, aShape, dtA[0]);
                    const float c1 = v.a[1].process (s.aWave, aShape, dtA[1]);
                    const float c2 = v.a[2].process (s.aWave, aShape, dtA[2]);
                    aL = (c0 + side * (0.85f * c1 + 0.35f * c2)) * aNorm * aG;
                    aR = (c0 + side * (0.85f * c2 + 0.35f * c1)) * aNorm * aG;
                }
                else for (int q = 0; q < 3; ++q) { v.a[q].phase += dtA[q]; if (v.a[q].phase >= 1.0f) v.a[q].phase -= 1.0f; }

                // Engine B
                float bL = 0.0f, bR = 0.0f;
                for (int q = 0; q < 5; ++q)
                {
                    float& ph = v.bPhase[q];
                    if (bG > 0.0001f && gB[q] > 0.0f)
                    {
                        const float pw = ph < warpK ? 0.5f * ph / warpK : 0.5f + 0.5f * (ph - warpK) / (1.0f - warpK);
                        const float w = WaveReader::read (tview, bPos, pw, dtB[q] * 0.5f / warpK) * gB[q];
                        bL += w * (1.0f - bPanK[q] * s.spread * 0.8f);
                        bR += w * (1.0f + bPanK[q] * s.spread * 0.8f);
                    }
                    ph += dtB[q]; if (ph >= 1.0f) ph -= 1.0f;
                }
                bL *= bNorm * bG * 0.5f; bR *= bNorm * bG * 0.5f;

                // breath
                float nzL = 0.0f, nzR = 0.0f;
                if (nG > 0.0001f)
                {
                    const float w1 = v.noise.next(), w2 = v.noise.next();
                    const float x = v.noiseLP.lowpass (v.noiseHP.highpass (0.7f * w1 + 0.3f * w2));
                    nzL = x * nG * 0.9f; nzR = (0.8f * x + 0.2f * w2 * 0.3f) * nG * 0.9f;
                }

                const float xL = (aL + bL + nzL) * driveGain * inTrim;
                const float xR = (aR + bR + nzR) * driveGain * inTrim;
                const float yL = v.lad[0].process (xL, G, k) * driveComp;
                const float yR = v.lad[1].process (xR, G, k) * driveComp;

                const float env = v.amp.next() * velGain * ampMod * voiceHeadroom;
                const float pl = v.panL + (v.panLT - v.panL) * t, pr = v.panR + (v.panRT - v.panR) * t;
                outL[i] += yL * env * pl;
                outR[i] += yR * env * pr;
            }
            v.aGain = v.aGainT; v.bGain = v.bGainT; v.airG = v.airGT;
            v.cutLog = v.cutLogTarget; v.panL = v.panLT; v.panR = v.panRT;

            if (v.amp.stage == Env::idle) { v.active = false; v.gate = false; }

            // remember modulated knob positions of the newest voice for the UI
            if (&v == newestVoice())
            {
                lastDst[(int) Dst::aDetune] = aDet; lastDst[(int) Dst::aShape] = aShape; lastDst[(int) Dst::aLevel] = aLev;
                lastDst[(int) Dst::bPos] = bPos; lastDst[(int) Dst::bWarp] = bWarp; lastDst[(int) Dst::bUnison] = bUni;
                lastDst[(int) Dst::bLevel] = bLev; lastDst[(int) Dst::air] = airLev; lastDst[(int) Dst::res] = res;
                lastDst[(int) Dst::drive] = drive; lastDst[(int) Dst::cutoff] = cutN;
                lastDst[(int) Dst::aTune] = clamp01 (s.aTune + m[(int) Dst::aTune] * 0.5f);
                lastCutHz = std::exp2 (v.cutLog);
            }
        }

        std::array<float, numDst> lastDst {};
        float lastCutHz = 1000.0f;

        void publishDisplay()
        {
            int active = 0;
            for (int i = 0; i < maxVoices; ++i)
            {
                const auto& v = voices[(size_t) i];
                if (v.active) ++active;
                display.voiceLevel[(size_t) i].store (v.active ? v.amp.level : 0.0f, std::memory_order_relaxed);
                display.voicePitch[(size_t) i].store (v.pitch, std::memory_order_relaxed);
                display.voicePress[(size_t) i].store (v.press, std::memory_order_relaxed);
                display.voicePlate[(size_t) i].store (v.active ? v.plate : -1, std::memory_order_relaxed);
            }
            display.activeVoices.store (active);

            for (int p = 0; p < numPlates; ++p)
            {
                float lv = 0.0f;
                if (p < numNotePlates)
                {
                    for (auto& v : voices) if (v.active && v.plate == p) lv = std::max (lv, v.amp.level * (0.6f + 0.4f * v.press));
                    if (plates[(size_t) p].down) lv = std::max (lv, 0.35f + 0.65f * plates[(size_t) p].press);
                }
                else lv = plates[(size_t) p].press;
                display.plateLevel[(size_t) p].store (lv, std::memory_order_relaxed);
                display.plateLocked[(size_t) p].store (plates[(size_t) p].locked, std::memory_order_relaxed);
            }

            const Voice* nv = newestVoice();
            for (int sI = 1; sI < numSrc; ++sI)
            {
                const float val = nv != nullptr ? voiceSrc ((Src) sI, *nv) : globalSrc ((Src) sI);
                display.src[(size_t) sI].store (val, std::memory_order_relaxed);
            }
            for (int d = 0; d < numDst; ++d) display.dstNorm[(size_t) d].store (nv != nullptr ? lastDst[(size_t) d] : -1.0f, std::memory_order_relaxed);
            display.cutoffHzNow.store (lastCutHz, std::memory_order_relaxed);
            display.drumStep.store (drums.currentStep, std::memory_order_relaxed);
            display.bendNow.store (juce_clampBend (bend + uiBend.load()), std::memory_order_relaxed);
            display.drumKicks.store (drums.kickCount, std::memory_order_relaxed);
            display.drumHits.store (drums.hitCount, std::memory_order_relaxed);
        }
    };
}
