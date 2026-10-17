#pragma once

// A pedal-style audio looper that sits after the whole synth. It records what you hear,
// so switching presets doesn't change the loop: loop a pad, change patch, play a lead over it.
//
//   LOOP:  empty -> record -> play -> overdub -> play -> overdub ...   (stopped -> play)
//   STOP:  recording -> closes the loop and stops;  playing / overdubbing -> stops
//   UNDO:  takes back the last overdub;  CLEAR: empties it
//
// Seamless loop points: the looper always keeps the last half second of what you played. When a
// loop closes, its last half second is crossfaded into that "pre-roll" (the sound from just before
// the loop started), so the end runs straight into the start with no gap or jump. Overdubs fade in
// over a moment when you punch in, and when you punch out they keep recording while fading away,
// so a held chord tapers off instead of being chopped at the same spot every time round.

#include "Common.h"
#include <atomic>
#include <vector>

namespace zen
{
    class Looper
    {
    public:
        enum State { empty = 0, recording, playing, overdubbing, stopped,
                     armed,      // drums running: waiting for the next bar line to start recording
                     closing };  // drums running: recording until the next bar line, then playing

        void prepare (float sampleRate, float maxSeconds = 45.0f)
        {
            fs = sampleRate;
            maxLen = std::max (1, (int) (maxSeconds * fs));
            for (auto* b : { &bufL, &bufR, &undoL, &undoR }) b->assign ((size_t) maxLen, 0.0f);
            fadeLen = std::max (8, (int) (0.006f * fs));
            xfadeMax = std::max (16, (int) (xfadeSeconds * fs));
            for (auto* b : { &ringL, &ringR, &preL, &preR }) b->assign ((size_t) xfadeMax, 0.0f);
            ringPos = 0;
            dubInStep  = 1.0f / std::max (1.0f, dubInSeconds * fs);
            dubOutStep = 1.0f / std::max (1.0f, dubOutSeconds * fs);
            reset();
        }

        void reset()
        {
            state = empty; len = 0; pos = 0; hasUndo = false; touched = 0; dub = 0.0f;
            gain.snap (0.0f);
        }

        // Thread-safe requests from the UI (or MIDI); applied at the start of the next block.
        void requestLoop()  noexcept { reqLoop.fetch_add (1); }
        void requestStop()  noexcept { reqStop.fetch_add (1); }
        void requestUndo()  noexcept { reqUndo.fetch_add (1); }
        void requestClear() noexcept { reqClear.fetch_add (1); }

        // For the UI
        std::atomic<int> displayState { empty };
        std::atomic<float> displayPos { 0.0f }, displaySeconds { 0.0f };
        std::atomic<bool> displayCanUndo { false };

        // In-place: records io (when recording/overdubbing) and adds the loop to it.
        // barOffsets: where bar lines fall in this block when the drums are running (synced), so
        // recording starts and stops on the beat and every loop is a whole number of bars.
        void process (float* L, float* R, int n, float level,
                      const int* barOffsets = nullptr, int numBarOffsets = 0, bool synced = false) noexcept
        {
            sync = synced;
            handleRequests();
            if (! synced && state == armed)   beginRecording();
            if (! synced && state == closing) closeLoop (playing);
            if (bufL.empty()) return;
            gain.setTime (0.01f * fs);
            gain.target = (state == playing || state == overdubbing || state == recording) ? clamp01 (level) : 0.0f;
            if (state == recording || state == closing || state == armed) gain.target = 0.0f;   // nothing to play back yet

            int nextBar = 0;
            for (int i = 0; i < n; ++i)
            {
                if (nextBar < numBarOffsets && barOffsets[nextBar] == i)
                {
                    ++nextBar;
                    if (state == armed)        beginRecording();
                    else if (state == closing) closeLoop (playing);
                }
                const float g = gain.next();
                const float inL = L[i], inR = R[i];
                // always remember the last half second (the pre-roll for the next loop)
                ringL[(size_t) ringPos] = inL; ringR[(size_t) ringPos] = inR;
                if (++ringPos >= xfadeMax) ringPos = 0;
                // the overdub's record level: a short fade in on punch-in, a long taper after punch-out
                if (state == overdubbing) dub = std::min (1.0f, dub + dubInStep);
                else if (dub > 0.0f)      dub = std::max (0.0f, dub - dubOutStep);
                switch (state)
                {
                    case closing:
                    case recording:
                        bufL[(size_t) pos] = inL; bufR[(size_t) pos] = inR;
                        if (++pos >= maxLen) closeLoop (playing);
                        break;

                    case overdubbing:
                    case playing:
                    {
                        const float oL = bufL[(size_t) pos], oR = bufR[(size_t) pos];
                        L[i] += oL * g; R[i] += oR * g;
                        if (dub > 0.0f) dubSample (oL, oR, inL, inR);
                        if (++pos >= len) pos = 0;
                        break;
                    }

                    case stopped:
                        if ((g > 1.0e-4f || dub > 0.0f) && len > 0)   // let the fade-out (and an overdub's taper) finish
                        {
                            const float oL = bufL[(size_t) pos], oR = bufR[(size_t) pos];
                            L[i] += oL * g; R[i] += oR * g;
                            if (dub > 0.0f) dubSample (oL, oR, inL, inR);
                            if (++pos >= len) pos = 0;
                        }
                        break;

                    case armed: case empty: default: break;
                }
            }

            displayState.store ((int) state, std::memory_order_relaxed);
            displayPos.store ((state == recording || state == closing) ? (float) pos / (float) maxLen
                                                 : (len > 0 ? (float) pos / (float) len : 0.0f), std::memory_order_relaxed);
            displaySeconds.store ((float) ((state == recording || state == closing) ? pos : len) / fs, std::memory_order_relaxed);
            displayCanUndo.store (hasUndo, std::memory_order_relaxed);
        }

        State getState() const noexcept { return state; }
        int length() const noexcept     { return len; }

        static constexpr float xfadeSeconds = 0.5f, dubInSeconds = 0.08f, dubOutSeconds = 0.6f;

    private:
        std::vector<float> bufL, bufR, undoL, undoR;
        std::vector<float> ringL, ringR, preL, preR;   // the last half second, and a copy taken when recording starts
        int xfadeMax = 1, ringPos = 0;
        float dub = 0.0f, dubInStep = 0.001f, dubOutStep = 0.001f;

        void dubSample (float oL, float oR, float inL, float inR) noexcept
        {
            if (touched < len) { undoL[(size_t) pos] = oL; undoR[(size_t) pos] = oR; ++touched; }
            // layers add up cleanly (the output limiter looks after the total); just keep it bounded
            bufL[(size_t) pos] = std::max (-4.0f, std::min (4.0f, oL + inL * dub));
            bufR[(size_t) pos] = std::max (-4.0f, std::min (4.0f, oR + inR * dub));
        }

        void beginRecording() noexcept
        {
            // keep what was playing just before the loop starts, oldest first
            for (int k = 0; k < xfadeMax; ++k)
            {
                const int j = (ringPos + k) % xfadeMax;
                preL[(size_t) k] = ringL[(size_t) j]; preR[(size_t) k] = ringR[(size_t) j];
            }
            state = recording; pos = 0; len = 0; hasUndo = false; dub = 0.0f;
        }
        float fs = 48000.0f;
        int maxLen = 1, len = 0, pos = 0, fadeLen = 256, touched = 0;
        bool hasUndo = false, sync = false;
        State state = empty;
        Smoother gain;
        std::atomic<int> reqLoop { 0 }, reqStop { 0 }, reqUndo { 0 }, reqClear { 0 };

        void handleRequests() noexcept
        {
            if (reqClear.exchange (0) > 0) { state = empty; len = 0; pos = 0; hasUndo = false; dub = 0.0f; }
            for (int k = reqLoop.exchange (0); k > 0; --k) pressLoop();
            if (reqStop.exchange (0) > 0) pressStop();
            if (reqUndo.exchange (0) > 0) undo();
        }

        void pressLoop() noexcept
        {
            switch (state)
            {
                case empty:       if (sync) { state = armed; pos = 0; len = 0; hasUndo = false; } else beginRecording(); break;
                case armed:       state = empty; break;                        // changed your mind
                case recording:   if (sync) state = closing; else closeLoop (playing); break;
                case closing:     break;
                case playing:     startOverdub(); break;
                case overdubbing: state = playing; break;
                case stopped:     state = playing; pos = 0; dub = 0.0f; gain.snap (0.0f); break;
            }
        }

        void pressStop() noexcept
        {
            if (state == recording || state == closing) closeLoop (stopped);
            else if (state == armed) state = empty;
            else if (state == playing || state == overdubbing) state = stopped;
        }

        void startOverdub() noexcept
        {
            state = overdubbing;
            if (dub > 0.0f && hasUndo) return;   // punched back in while the last one was still tapering: same take
            touched = 0;
            hasUndo = true;
            undoStart = pos;
        }

        void undo() noexcept
        {
            if (! hasUndo || len <= 0) return;
            // put back every sample the last overdub touched
            for (int k = 0; k < touched; ++k)
            {
                const int i = (undoStart + k) % len;
                bufL[(size_t) i] = undoL[(size_t) i]; bufR[(size_t) i] = undoR[(size_t) i];
            }
            hasUndo = false;
            touched = 0;
            dub = 0.0f;
            if (state == overdubbing) state = playing;
        }

        void closeLoop (State next) noexcept
        {
            len = std::max (fadeLen * 4, pos);
            const int x = std::min (xfadeMax, len / 4);

            // How alike are the loop's end and the pre-roll? A steady, simple tone (an organ, a pure
            // pad) is the same waveform in both, and crossfading two copies out of step would hollow
            // it out for a moment. When the loop isn't locked to the drums, trim up to ~15 ms off the
            // end so the two line up in phase; then pick crossfade gains that keep the level steady
            // whether the two are alike (r near 1) or unrelated (r near 0).
            const int w = std::min (x, (int) (0.05f * fs));
            auto similarity = [this, w] (int end) noexcept
            {
                double ab = 0.0, aa = 1.0e-12, bb = 1.0e-12;
                for (int k = 0; k < w; k += 4)
                {
                    const size_t i = (size_t) (end - w + k), j = (size_t) (xfadeMax - w + k);
                    const double a1 = bufL[i] + bufR[i], b1 = preL[j] + preR[j];
                    ab += a1 * b1; aa += a1 * a1; bb += b1 * b1;
                }
                return (float) (ab / std::sqrt (aa * bb));
            };
            float r = similarity (len);
            if (! sync && w > 64)
            {
                const int maxTrim = std::min ((int) (0.015f * fs), len / 8);
                // coarse search, then fine around the best (keeps this cheap on the audio thread)
                int best = len; float bestC = r;
                for (int end = len - 4; end >= len - maxTrim; end -= 4)
                    if (const float c = similarity (end); c > bestC) { bestC = c; best = end; }
                for (int end = std::min (len, best + 3); end >= std::max (len - maxTrim, best - 3); --end)
                    if (const float c = similarity (end); c > bestC) { bestC = c; best = end; }
                if (bestC > r + 0.02f) { r = bestC; len = best; }
            }
            const float rr = std::max (0.0f, std::min (1.0f, r));

            // Crossfade the end of the loop into the pre-roll: by the last sample the loop is playing
            // what came just before its first sample, so it wraps with no gap.
            for (int k = 0; k < x; ++k)
            {
                const float t = ((float) k + 0.5f) / (float) x;
                float a = std::cos (t * 1.5707963f), b = std::sin (t * 1.5707963f);
                const float norm = 1.0f / std::sqrt (a * a + b * b + 2.0f * a * b * rr);
                a *= norm; b *= norm;
                const size_t i = (size_t) (len - x + k), j = (size_t) (xfadeMax - x + k);
                bufL[i] = bufL[i] * a + preL[j] * b;
                bufR[i] = bufR[i] * a + preR[j] * b;
            }
            pos = 0;
            state = next;
            gain.snap (0.0f);
        }

        int undoStart = 0;
    };
}
