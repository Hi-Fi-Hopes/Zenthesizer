# Zenthesizer

An organic, ethereal VST3 synth for Ableton Live, laid out on a cross-cut tree slab.
Built with JUCE. GitHub builds the Windows `.vst3` on every push to `main`.

## Sound

- **Engine A (analog):** saw, square, triangle or sine, three detunable oscillators per voice.
  Shape softens the saw, sets the square's pulse width, folds the triangle, or adds self-FM to the sine.
- **Engine B (wavetable):** four tables (Air, Glass, Vox, Reed), eight frames each.
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

## Quick modulation

Click a mod source (ENV1, LFO1, PRESS...) on the SYNTH page, or right-click a timbre plate.
Knobs that can be modulated get a coloured halo. Drag one to set how much the source moves it.
Double-click removes it. Click the source again (or bare wood) when done.
The full list of 12 slots is on the MOD page.

## Building

Push to `main`. GitHub Actions builds the Windows VST3, attaches it to the run and commits
`Zenthesizer-VST3-Windows.zip` to the `builds` branch.

DSP tests (no JUCE needed):

```
g++ -std=c++17 -O2 -ISource tests/dsp_test.cpp -o zt && ./zt
```

Fonts: Instrument Serif and Instrument Sans, SIL Open Font License (see `Resources/`).
