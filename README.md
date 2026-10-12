# Zenthesizer

An organic, ethereal VST3 synth for Ableton Live, laid out on a cross-cut tree slab.
Built with JUCE. GitHub builds the Windows `.vst3` on every push to `main`.

## Sound

- **Engine A (analog):** saw, square, triangle or sine, three detunable oscillators per voice.
  Shape softens the saw, sets the square's pulse width, folds the triangle, or adds self-FM to the sine.
- **Engine B (wavetable):** four tables (Air, Glass, Vox, Reed), eight frames each, plus **Own** (your sandbox).
  Position scans the frames, Warp bends the phase, Unison stacks up to five detuned copies.
- **Air:** breath noise.
- **Filter:** 4-pole ladder with drive. ENV sweeps it with Env 1.
- **Voices:** 8, with glide.
- **Space:** chorus, ping-pong delay (free or synced) and a shimmer reverb.

## Pages

| Tab | What's there |
|---|---|
| SYNTH | the slab: engines, filter/amp ring, mod sources, the porthole |
| MOD | Env 1/2, LFO 1/2, Random, key tracking, velocity, the 12 mod slots |
| WAVES | the wavetable sandbox for Engine B's OWN table |
| BEAT | 16-step pattern, 29 sounds, kits, synth patches on rows, grooves, tempo, swing |
| FX | chorus, echo, shimmer reverb, air, glide, spread, volume |
| PLAY | root, scale, octave for the note plates; arpeggiator; Launchkey pads and Learn |
| STAGE | the porthole, large, for playing |

## Plates and the Launchkey

- The 12 **note plates** play notes in the chosen scale. Pressure (poly aftertouch) opens the filter.
- The 4 **timbre plates** (A-D) are modulation sources. Their pressure moves whatever they're assigned to
  (default: A bloom, B morph, C echoes, D space).
- By default pad notes C1-B1 (36-47) play plates 1-12 and C2-D#2 (48-51) play A-D.
  If your pads send other notes: PLAY tab, LEARN, click a plate, hit a pad.
- You can also click the plates; drag up while holding for more pressure.

## Pitch and mod

Two curved strips along the slab's edges, on every page: **PITCH** (left) bends and springs back
to the middle when you let go; **MOD** (right) works like a mod wheel and stays where you leave it.
They follow your keyboard's pitch and mod wheels too (whichever moved last wins for mod).
The bend range (1 to 24 semitones, default 2) is on the PLAY page and applies to both.
In the presets the mod wheel opens the filter; assign it elsewhere with the WHEEL source.

## Quick modulation

Click a mod source (ENV1, LFO1, PRESS...) on the SYNTH page, or right-click a timbre plate.
Knobs that can be modulated get a coloured halo. Drag one to set how much the source moves it.
Double-click removes it. Click the source again (or bare wood) when done.
The full list of 12 slots is on the MOD page.

## Looper

Bottom right of the slab, on every page. It records what you hear (after the effects),
so you can loop a pad, switch presets and play a lead over it.

- **LOOP:** first tap records, next tap closes the loop and plays, then taps toggle overdub / play.
  Light: red recording, amber overdubbing, green playing; the ring shows where the loop is.
- **STOP:** stops (LOOP plays it again from the start). Pressed while recording, it closes the loop stopped.
- **UNDO:** tap to take back the last overdub, hold to clear the loop.
- **LOOP LVL** (bottom left): playback level. Loops last up to 45 s and aren't saved with the set.
- With the BEAT drums running, recording starts and stops on bar lines (see BEAT).

## BEAT: percussion

Eight rows and a 16-step pattern. Tap a step: off -> on -> accent -> off; drag along a row to paint.
Tap a row's name to select it (and hear it); LEVEL / TUNE / DECAY shape the selected row.

- **SOUND** picks what the selected row plays: 29 synthesized sounds in six groups (kicks, snares &
  claps, woods, shakers & hats, hand drums, tonal), or a synth patch: *From a preset*, *From your
  presets*, or *The sound you're playing now* (a snapshot of the current settings, tweaks included).
- A patch row plays one note of that patch (held ~2 s, up to 3 s with its tail), captured when you
  pick it. It starts on C3; on a patch row TUNE picks the note (C1 to C6) and letting go of TUNE
  captures it again at that note. DECAY shortens it. A small brass dot marks patch rows.
- **KITS** sets all eight rows at once (Organic, Hand Drums, Lo-Fi, Minimal, Temple, Rain Garden,
  With Bell) and keeps your pattern. The default Organic kit has a raindrop where the bell was.
- Sounds and captured patches are saved with the session and captured again when it reopens.
GROOVES loads one of 15 ready-made patterns. In Ableton the drums follow Ableton's tempo and,
while Ableton plays, its position; in the app TEMPO sets the speed.

The drums are separate from the presets (changing sound doesn't change the beat) and play after
the looper, so loops don't record them. **While the drums run, the looper locks to the bar:**
LOOP waits for the next bar line to start recording (light blinks), and a second LOOP keeps
recording to the next bar line, so every loop is a whole number of bars and stays in time.

## Locking a timbre plate

Double-tap a timbre plate (A-D), on screen or on its Launchkey pad, to lock it on at the
hardest point of that press. A twin ring shows it's locked. Press it again to change the
held value; double-tap again to unlock.

## The wavetable sandbox (WAVES)

- **DRAW:** draw one cycle with the mouse or a finger. **HARMONICS:** drag 32 bars that set each overtone.
- **Keyframes:** up to 8. **+ FRAME** copies the current one; Engine B's POSITION morphs through them.
- **SHAPES:** start from a basic shape, or copy all 8 frames of a built-in table.
- **SMOOTH** rounds off a drawn shape. **IMPORT** reads a WAV/AIFF/FLAC:
  a single cycle becomes one keyframe, a wavetable file (2048 samples per frame) up to 8,
  and any other audio is sliced into 8 looped snippets.
- **SAVE / LOAD:** your wave library (standard wavetable WAVs). Your waves are also saved with the
  Ableton set and with your own presets (preset menu: "Save current sound as...").
- Every wave is band-limited per octave, so even jagged drawings don't alias on high notes.

## Android app

The same synth, as a tablet app (`Zenthesizer.jucer`, built by `.github/workflows/android.yml`).
Plays through the tablet's audio, takes MIDI from USB (Launchkey over USB-C) or Bluetooth (the
BLUETOOTH button, top left). AUDIO (top right) picks the output and buffer size.
Long-press a timbre plate to assign it (instead of right-click).
Zoom: two fingers on bare wood pinch (up to 4x), one finger on the wood drags, a double-tap on the wood
zooms in, FIT (bottom left) zooms back out. Touches on plates and knobs never zoom.

The APK is signed with `android/zen.keystore` (password `zenthesizer`), kept in the repo so every
build can install over the last one.

## Building

Push to `main`. GitHub Actions builds the Windows VST3, attaches it to the run and commits
`Zenthesizer-VST3-Windows.zip` to the `builds` branch.

DSP tests (no JUCE needed):

```
g++ -std=c++17 -O2 -ISource tests/dsp_test.cpp -o zt && ./zt
```

Fonts: Instrument Serif and Instrument Sans, SIL Open Font License (see `Resources/`).
