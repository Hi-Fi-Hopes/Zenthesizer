#pragma once

// A pedal-style audio looper that sits after the whole synth. It records what you hear,
// so switching presets doesn't change the loop: loop a pad, change patch, play a lead over it.
//
//   LOOP:  empty -> record -> play -> overdub -> play -> overdub ...   (stopped -> play)
//   STOP:  recording -> closes the loop and stops;  playing / overdubbing -> stops
//   UNDO:  takes back the last overdub;  CLEAR: empties it

#include "Common.h"
#include <atomic>
#include <vector>

namespace zen
{
    class Looper
    {
    public:
        enum State { empty = 0, recording, playing, overdubbing, stopped };

        void prepare (float sampleRate, float maxSeconds = 45.0f)
        {
            fs = sampleRate;
            maxLen = std::max (1, (int) (maxSeconds * fs));
            for (auto* b : { &bufL, &bufR, &undoL, &undoR }) b->assign ((size_t) maxLen, 0.0f);
            fadeLen = std::max (8, (int) (0.006f * fs));
            reset();
        }

        void reset()
        {
            state = empty; len = 0; pos = 0; hasUndo = false; touched = 0;
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
        void process (float* L, float* R, int n, float level) noexcept
        {
            handleRequests();
            if (bufL.empty()) return;
            gain.setTime (0.01f * fs);
            gain.target = (state == playing || state == overdubbing || state == recording) ? clamp01 (level) : 0.0f;
            if (state == recording) gain.target = 0.0f;   // nothing to play back yet

            for (int i = 0; i < n; ++i)
            {
                const float g = gain.next();
                const float inL = L[i], inR = R[i];
                switch (state)
                {
                    case recording:
                        bufL[(size_t) pos] = inL; bufR[(size_t) pos] = inR;
                        if (++pos >= maxLen) closeLoop (playing);
                        break;

                    case overdubbing:
                    {
                        if (touched < len) { undoL[(size_t) pos] = bufL[(size_t) pos]; undoR[(size_t) pos] = bufR[(size_t) pos]; ++touched; }
                        const float oL = bufL[(size_t) pos], oR = bufR[(size_t) pos];
                        L[i] += oL * g; R[i] += oR * g;
                        bufL[(size_t) pos] = softClip (oL + inL);
                        bufR[(size_t) pos] = softClip (oR + inR);
                        if (++pos >= len) pos = 0;
                        break;
                    }

                    case playing:
                        L[i] += bufL[(size_t) pos] * g; R[i] += bufR[(size_t) pos] * g;
                        if (++pos >= len) pos = 0;
                        break;

                    case stopped:
                        if (g > 1.0e-4f && len > 0)   // let the fade-out finish
                        {
                            L[i] += bufL[(size_t) pos] * g; R[i] += bufR[(size_t) pos] * g;
                            if (++pos >= len) pos = 0;
                        }
                        break;

                    case empty: default: break;
                }
            }

            displayState.store ((int) state, std::memory_order_relaxed);
            displayPos.store (state == recording ? (float) pos / (float) maxLen
                                                 : (len > 0 ? (float) pos / (float) len : 0.0f), std::memory_order_relaxed);
            displaySeconds.store ((float) (state == recording ? pos : len) / fs, std::memory_order_relaxed);
            displayCanUndo.store (hasUndo, std::memory_order_relaxed);
        }

        State getState() const noexcept { return state; }
        int length() const noexcept     { return len; }

    private:
        std::vector<float> bufL, bufR, undoL, undoR;
        float fs = 48000.0f;
        int maxLen = 1, len = 0, pos = 0, fadeLen = 256, touched = 0;
        bool hasUndo = false;
        State state = empty;
        Smoother gain;
        std::atomic<int> reqLoop { 0 }, reqStop { 0 }, reqUndo { 0 }, reqClear { 0 };

        void handleRequests() noexcept
        {
            if (reqClear.exchange (0) > 0) { state = empty; len = 0; pos = 0; hasUndo = false; }
            for (int k = reqLoop.exchange (0); k > 0; --k) pressLoop();
            if (reqStop.exchange (0) > 0) pressStop();
            if (reqUndo.exchange (0) > 0) undo();
        }

        void pressLoop() noexcept
        {
            switch (state)
            {
                case empty:       state = recording; pos = 0; len = 0; hasUndo = false; break;
                case recording:   closeLoop (playing); break;
                case playing:     startOverdub(); break;
                case overdubbing: state = playing; break;
                case stopped:     state = playing; pos = 0; gain.snap (0.0f); break;
            }
        }

        void pressStop() noexcept
        {
            if (state == recording) closeLoop (stopped);
            else if (state == playing || state == overdubbing) state = stopped;
        }

        void startOverdub() noexcept
        {
            state = overdubbing;
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
            if (state == overdubbing) state = playing;
        }

        void closeLoop (State next) noexcept
        {
            len = std::max (fadeLen * 4, pos);
            // short fades at both ends so the loop point doesn't click
            const int f = std::min (fadeLen, len / 4);
            for (int i = 0; i < f; ++i)
            {
                const float g = (float) i / (float) f;
                bufL[(size_t) i] *= g; bufR[(size_t) i] *= g;
                bufL[(size_t) (len - 1 - i)] *= g; bufR[(size_t) (len - 1 - i)] *= g;
            }
            pos = 0;
            state = next;
            gain.snap (0.0f);
        }

        int undoStart = 0;
    };
}
