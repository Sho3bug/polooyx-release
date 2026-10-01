# POLOOYX RELEASE ENGINE — known limitations (milestone 1)

Honest list of what is simplified or not yet built. Nothing here is faked in the plugin; where a method is an approximation the UI says so.

## Not in milestone 1 (planned for milestone 2)
* **Reference track** loading, reference level match and reference comparison (spec §41–42).
* **Section analysis** (SECTION A / B / C loudness, peak, spectrum changes — spec §46).
* **LAB extras**: FFT window / overlap choice, phase scope, per-band correlation display, editable gain-staging chart. LAB currently shows
  live measurements, per-stage gain reduction, latency/CPU and the full analysis table.
* **macOS / AU** builds (the code is portable JUCE; only the CI job is missing).
* UI scale presets (100–200 %): the window resizes freely with a fixed aspect ratio (960×600 … 2560×1600) instead.

## How the analysis works — and what that implies
* **ANALYZE POLO listens in real time.** A plugin cannot read the song file, so you play the mix and POLO captures it (≤ 60 s at
  ≤ 96 kHz, ≤ 30 s above). Analyse the whole song or at least its loudest part; a quiet intro alone gives misleading loudness.
* **Capture starts on the first non-silent sample while the transport plays** and ends when you press STOP, the buffer is full, or
  playback stops after at least 10 s.
* **Diagnosis is rule-based**, not AI. Every threshold is in `docs/DSP_NOTES.md`. Measurements cannot prove that something sounds good
  or bad — that is why the wording is POSSIBLE / LIKELY / CHECK / DETECTED.
* **Tonal judgements are relative to the mix's own tonal line**, not to a genre target curve. A deliberately dark or bright master can be
  flagged as POSSIBLY DARK / BRIGHT; POLO IT never boosts to "fix" it.
* **VOCAL REGION and MASKING are frequency-domain estimates.** There is no source separation, so no instrument is named.
* **True peak is a 4x-interpolation estimate** (BS.1770 tolerance +0.2 / −0.4 dB at 48 kHz). The limiter detects at 4x; the meter can
  use 2x / 4x / 8x (SETTINGS, applies when playback restarts).
* **LRA** needs at least ~4 s of audio; very short captures show 0 LU.

## DSP trade-offs
* **Crossovers rotate phase.** Multiband, per-band width and LOW FREQUENCY MONO use Linkwitz-Riley filters (magnitude-flat, not
  linear-phase). Each is bypassed exactly while idle; when engaged, the low end is phase-shifted around the crossover like any
  minimum-phase mastering crossover. Linear-phase mode is not implemented.
* **Changing the oversampling factor while playing** restarts that oversampling path (a brief discontinuity). Set QUALITY before playback.
* **Limiter MIX < 100 %** blends in the unlimited signal, so peaks can exceed the ceiling (only the sample clamp remains). FINAL CHECK
  reports it.
* **POLO IT limiter drive is capped at +12 dB.** Very quiet or extremely dynamic mixes may not reach a loud target — the notes say so.
* **Clipper output** can sit up to ~0.7 dB above its ceiling after the anti-alias filter (filter overshoot); the true-peak limiter after it
  catches this.
* **SMALL SPEAKER** is a filtered approximation, not a model of a specific device.
* **MONO / SMALL SPEAKER change the plugin output** (they are monitoring switches). The sidebar warns while they are active and FINAL
  CHECK reminds you; switch back to FULL before exporting.

## Testing scope
FL Studio cannot run in the build environment. Host behaviour is covered by the offline harness (`Tests/TestMain.cpp`, 254 checks) and by
pluginval at strictness 10 (state restore, automation, threading, bus layouts, fuzzing) on every CI build. The plugin has not yet been
played in FL Studio by a human — that is the next step.
