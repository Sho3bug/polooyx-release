# POLOOYX RELEASE ENGINE — DSP & analysis notes

## 1. Architecture
```
            ┌──────────── audio thread ─────────────┐        ┌──── background thread ────┐     ┌──── UI thread ────┐
host ─► FIFO (32-sample grid) ─► processChunk ─► host     │ AutoMasterEngine           │     │ SpectrumAnalyzer  │
            │  input meters / capture (ANALYZE POLO)    │   analyzeMix → diagnose    │     │ (FFT of FIFO data)│
            │  MasterChain.process (resolve(snapshot))  │   recommend → SafetyEngine │     │ panels, meters    │
            │  output meters, level match, A/B, monitor │   render via MasterChain   │     │                   │
            └───────────────────────────────────────────┘   analyse AFTER / FINAL    │     └───────────────────┘
                     atomics / lock-free FIFOs ◄────────────────────────────────────►  results (mutex, msg thread)
```
* **DSP independent of UI**: everything in `Source/DSP` and `Source/Analysis` is plain C++ with no JUCE GUI dependency.
* **One chain, two uses**: `MasterChain` + `resolve()` run live and inside POLO IT / FINAL CHECK renders. The test suite proves the
  live plugin output and the offline render are bit-identical, so POLO IT verifies the processing you will actually hear.
* **Fixed 32-sample grid**: the processor feeds the chain through a 32-sample FIFO (+32 samples latency). Output is bit-identical at any
  host buffer size, parameters are smoothed once per grid step (30 ms one-pole) and interpolated per sample inside modules.
* **Real-time safety**: no allocation, locks or file I/O on the audio thread. The capture buffer is allocated in `prepareToPlay`; meters
  go out through atomics; the analyser receives samples through lock-free FIFOs and does its FFT on the UI thread; POLO IT runs on a
  worker thread and its result is applied on the message thread (one undo transaction).

## 2. Modules
| Module | Implementation | Neutral = bit-transparent? |
|---|---|---|
| Input / DC / subsonic | gain ramp; 1-pole DC blocker (~4 Hz); 4th-order Butterworth HP 10–40 Hz. DC and subsonic are **off by default** and switched on by POLO IT only when DC offset or infrasonic energy is measured (they rotate the phase of an 808 otherwise). | yes |
| Master EQ | 8 TPT state-variable filters (Simper) — bell / shelves / HP / LP, stable under modulation, exact bilinear response used for the curve display. TONE adds a see-saw tilt (low shelf 250 Hz −x, high shelf 1.7 kHz +x, ±2 dB). | yes (gain 0 bands bypassed) |
| Dynamic EQ | 4 bands; band-passed mid sidechain → dB envelope (attack/release) → soft-knee gain computer (3 dB knee) limited to RANGE → bell cut. Cut-only. | yes (threshold 0 dB) |
| Multiband | LR4 crossover tree with all-pass compensation (bands always sum to an all-pass, so turning one band down never boosts a neighbour). Stereo-linked compressors, optional slow auto gain. Because crossovers rotate phase, the module is **bypassed exactly until a band has a threshold < 0 dB, makeup or auto gain**; the filters keep running so engaging is seamless. | yes (idle) |
| Glue | Feed-forward, stereo-linked; detector = 10 ms mean-square of the louder channel (true RMS), log-domain attack/release, soft knee, makeup, parallel mix. | yes (threshold 0 dB) |
| Punch | fast/slow envelope difference on the mid = transientness. TRANSIENT ±5 dB on attacks, BODY ±3 dB on sustain, IMPACT +4 dB on the < 150 Hz band's *own* attacks (kick, not 808 tail). Stereo-linked, smoothed gains. | yes |
| Harmonic color | 2x/4x/8x FIR oversampling (JUCE half-band equiripple, integer latency). Shapers normalised to unity small-signal gain: TAPE tanh (+ HF-softened harmonics), TUBE odd soft curve + even (1−sech) bend (2nd harmonic dominant, DC removed), CONSOLE cubic, DIGITAL hard-soft knee, CLEAN none. Parallel MIX, ±3 dB TONE tilt, output trim. Idle → exact latency-matched bypass. | yes |
| Stereo / M-S | M=(L+R)/2, S=(L−R)/2. LOW FREQUENCY MONO = LR4 high-pass on S (mid untouched). Per-band width uses an LR4 tree (all-pass compensated) only when LOW/MID/HIGH widths differ; otherwise S×width with no filter in the path. Mid / side trims. | yes |
| POLOOYX character | AURA: harmonics generated only from > 7 kHz content + 12 kHz shelf. GRIT: only the *added* harmonics of a low-mid band-pass. GLASS: ≤ 2 dB dynamic 3.8 kHz smoothing + 10 kHz sheen. DEPTH / SPACE: short filtered reflections and all-pass-diffused ambience **in the side channel only** (cancel in mono). | yes (all 0) |
| Soft clipper | oversampled (shares the quality setting). SOFT knee from −2 dB under the ceiling, TAPE tanh to ceiling, HARD clamp. DRIVE 0 dB = idle. | yes |
| True-peak limiter | requirement from a 4x polyphase true-peak estimate (12 taps/phase, Kaiser) of the driven signal, covering both neighbours of each inter-sample peak; sliding-window minimum over LOOKAHEAD; box-car attack ramp; style-dependent release (TRANSPARENT program-dependent, PUNCH half-length ramp, DENSE fast + 0.5 dB pre-knee, MODERN adaptive). Audio delay is fixed at 10 ms + interpolator delay; the gain history is aligned to the lookahead in use. Final sample clamp at the ceiling. | n/a (only acts above the ceiling) |

**Latency** (48 kHz): 2 × 8x-oversampling latency (constant for every factor — lower factors are padded) + 10 ms + 6 samples (limiter)
+ 32 (grid) = 649 samples = 13.5 ms, reported to the host and identical at every setting.

**Quality**: ECO = no oversampling, NORMAL = 2x, HIGH = 4x, MAX = 8x (halved at 88.2/96 kHz, quartered at 176.4/192 kHz) when
OVERSAMPLING = AUTO. The analyser FFT follows quality (2048 … 16384, scaled with sample rate) unless set in SETTINGS.

## 3. Measurement
* **Loudness**: ITU-R BS.1770-4. K-weighting biquads are re-derived from their analogue prototypes for every sample rate (verified at
  44.1–192 kHz). Momentary 400 ms, short-term 3 s, updated every 100 ms. Integrated: 400 ms blocks, 75 % overlap, −70 LUFS absolute gate,
  −10 LU relative gate. LRA per EBU Tech 3342 (short-term, −20 LU relative gate, 10th–95th percentile). Gating uses 0.05 LU histograms
  (no allocation, < 0.01 LU error). Verified against EBU Tech 3341 cases 1–5 and Tech 3342 case 1 (±0.1 LU).
* **True peak**: 4x polyphase interpolation per BS.1770-4 Annex 2 (2x / 8x selectable for the meter). Labelled “dBTP (4x est.)”.
  Verified: fs/4 sine at 45° phase reads −6.02 dBTP while the sample peak is −9.03 dBFS.
* **Spectrum (analysis)**: Welch, Hann, 50 % overlap, FFT size chosen for ≤ 6 Hz resolution, mid + side power. Nine report bands
  (20–60 … 10–20 kHz), share of energy, per-octave level, centroid.
* **Tonal line / prominence**: 1/3-octave levels normalised to power per octave. The overall *tilt* is a least-squares slope over
  100 Hz–8 kHz. For local problems each 1/3-octave level is compared with a line fitted **without the surrounding ±0.7 octave** (so a bump
  cannot drag its own reference along), averaged over one octave → **prominence** (dB). A +6 dB bell (Q 1) reads ≈ +3 dB prominence,
  a balanced spectrum ≈ 0. Regions: low-mid 120–500 Hz, mid 500 Hz–1 kHz, presence 2–5 kHz.
* **Low end**: sub (20–60) and bass (60–120) per-octave level vs the 120–500 Hz average; side/mid energy below 120 Hz; infrasonic share
  (< 20 Hz).
* **Stereo**: correlation Σ(LR)/√(ΣL²ΣR²), width = side RMS / mid RMS, L/R balance.
* **Dynamics**: crest (sample peak − RMS), PLR (true peak − LUFS-I), LRA, short-term density, median 50 ms transient crest.
* **Clipping**: runs of ≥ 3 consecutive near-identical samples at ≥ −0.1 dBFS (flat tops); near-full-scale count; inter-sample overs.
* **DC offset**: only reported when the per-second means agree in sign and size across ≥ 90 % of the capture (an asymmetric kick has a
  non-zero short-term mean but is not an offset).

## 4. Diagnosis thresholds (what each wording means)
| Item | Rule |
|---|---|
| LOUDNESS | below / on / above target by ±1.5 LU |
| TRUE PEAK | SAFE ≤ ceiling; HIGH > ceiling; OVER > 0 dBTP (inter-sample when the sample peak ≤ 0) |
| DYNAMICS | LIKELY OVER-LIMITED: PLR < 6.5 and LUFS-I > −10 · DENSE: PLR < 8.5 or LRA < 2.5 · DYNAMIC: PLR > 12 · VERY DYNAMIC: PLR > 15 |
| LOW END | POSSIBLE EXCESS SUB: sub > +14 dB vs 120–500 Hz and > bass + 6 · POSSIBLY HEAVY: bass > +12 · LIGHT: sub < −6 and bass < −3 · INFRASONIC: > 2 % below 20 Hz |
| LOW-MID | POSSIBLE BUILDUP: prominence > 2 dB (reports the 1/3-octave where it peaks) |
| PRESENCE | POSSIBLY HARSH: prominence > 2 dB in 2–5 kHz · POSSIBLY RECESSED: 2–5 kHz average < −3 dB |
| HIGH END | POSSIBLY DULL: 10–20 kHz < −12 dB or tilt < −6 dB/oct · POSSIBLY BRIGHT: 10–20 kHz > +0.5 dB or tilt > 0 dB/oct (brighter than pink) |
| STEREO | PHASE ISSUE DETECTED: correlation < 0 · WIDE / CHECK MONO: < 0.25 · NEAR MONO: width < 6 % · OFF-CENTRE: L/R > 1.5 dB |
| LOW STEREO | WIDE LOW END: side/mid below 120 Hz > −12 dB |
| VOCAL REGION | estimate from 1–5 kHz average deviation (±2.5 dB) — not source separation |
| MASKING | regions whose prominence exceeds the low-mid / mid / presence thresholds — frequency-domain only, no instrument is named |

## 5. POLO IT
1. **BEFORE** = analysis of the captured input.
2. **Recommendations** (`RecommendationEngine.h`) — only when a threshold is crossed:
   * low-mid prominence > 2 dB → bell cut at the measured frequency, −0.6 × (prom − 1) dB × mode scale
   * mid prominence > 2.5 dB → bell cut 500–1000 Hz
   * excess sub → 40 Hz bell cut; bright → 9 kHz high-shelf cut ≤ 1.5 dB. **Dark / thin mixes are never boosted** (→ next steps)
   * presence prominence > 2 dB → dynamic EQ at the measured frequency, threshold = 90th percentile of that band's sidechain level − 1 dB
     (acts on the loudest ~10 % of moments), range ≤ 3 dB
   * DC offset → DC filter; infrasonic > 2 % → subsonic 18 Hz
   * side/mid below 120 Hz > −12 dB → LOW FREQUENCY MONO at 100 Hz (120 Hz if > −6 dB); correlation < 0.2 → narrow ≤ 15 %. **Never wider.**
   * glue only when PLR > 12 dB and LRA > 5 LU: parallel, mix ≤ the mode ceiling, threshold LUFS-I + 3 dB, 2:1, 30/200 ms
   * color (TAPE) / character only in CHARACTER and LOUD modes
3. **Safety engine** clamps every correction: EQ cut-only ≤ 3 dB, color ≤ 15 %, stereo change ≤ 15 % and never wider, glue mix ≤ 25 %,
   clipper ≤ 3 dB, limiter drive ≤ 12 dB, character ≤ 15 %; no-op corrections are dropped.
4. **Loudness**: render the captured mix through the real chain at drive 0, then iterate the limiter drive (secant-style, ≤ 6 renders) until
   the integrated loudness is within 0.25 LU of the target. If the result's PLR falls below the mode floor (SAFE 11, TRANSPARENT 10.5,
   BALANCED 9, CHARACTER 8.5, LOUD 7 dB) a bisection backs off and the note says the target was not reached on purpose.
   MIX / PREMASTER never pushes loudness; mixes already above the target are never turned down. LOUD / CHARACTER / BALANCED may add up to
   2 / 1 / 0.75 dB of soft clipping first when the PLR is > 11 dB.
5. **AFTER** render + FINAL CHECK. Every parameter that differs from the starting state is listed (CHANGES) with the measurement that
   caused it, then applied as one undo transaction.

| Mode | EQ scale / max cut | glue ≤ | color | character | clip ≤ | PLR floor |
|---|---|---|---|---|---|---|
| TRANSPARENT | 0.6 / 1.5 dB | 8 % | – | – | – | 10.5 dB |
| BALANCED | 1.0 / 2.5 dB | 15 % | – | – | 0.75 dB | 9 dB |
| CHARACTER | 1.0 / 2.5 dB | 20 % | 12 % | 10 % | 1 dB | 8.5 dB |
| LOUD | 0.8 / 2.0 dB | 25 % | 6 % | – | 2 dB | 7 dB |
| SAFE | 0.4 / 1.0 dB | 5 % | – | – | – | 11 dB |

## 6. A / B and monitoring
LEVEL MATCH tracks the K-weighted power of the processed and the (latency-aligned) original signal with a 3 s time constant and plays
ORIGINAL at the processed loudness (±24 dB). The export path (PROCESSED) is never gain-changed by level matching. Listen / bypass switches
are per-sample ramps over the 32-sample grid step plus parameter smoothing (click test in the report). MONO and SMALL SPEAKER act after
metering; SMALL SPEAKER is a filtered approximation (mono, 180 Hz 24 dB/oct HP, 7 kHz LP, +3 dB at 1.2 kHz) — not a speaker model.
