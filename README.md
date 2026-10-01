# POLOOYX — RELEASE ENGINE

Stereo mix analysis, correction, mastering and verification plugin (JUCE / C++, VST3 + Standalone, Windows x64).
Part of the POLOOYX ecosystem. The **Vocal Engine** shapes a vocal; the **Release Engine** finishes the record.

**ANALYZE → DIAGNOSE → CORRECT → VERIFY.**


## Download / install (no compiling)
Every push to `main` is built on GitHub's Windows servers, tested, validated with pluginval (strictness 10) and published to the
[`builds` branch](../../tree/builds) as `POLOOYX-RELEASE-ENGINE-Windows-x64.zip`. Unzip it and run
`Install POLOOYX RELEASE ENGINE.bat` — details in [packaging/INSTALL.txt](packaging/INSTALL.txt).

## Workflow
| Step | What happens |
|---|---|
| **ANALYZE POLO** | Listens to the incoming mix while you play it (up to 60 s at ≤ 96 kHz, 30 s above) and measures loudness (BS.1770-4), true peak, spectrum, low end, stereo, dynamics, clipping, DC. |
| **POLO DIAGNOSIS** | Every statement is tied to a measurement and a documented threshold (POSSIBLE / LIKELY / CHECK / DETECTED). Nothing is invented. |
| **NEXT STEPS** | What to check in the mix, most important first. |
| **POLO IT** | Recommends the smallest corrections, validates them against hard safety bounds, solves the limiter drive by rendering the captured mix through the *real* chain, verifies the result (BEFORE / AFTER) and lists every change. One UNDO step reverts it. If nothing needs fixing it changes nothing. |
| **A / B COMPARE** | PROCESSED vs ORIGINAL with **LEVEL MATCH** (the original is played at the processed loudness) — click-free. |
| **FINAL CHECK** | Renders the captured mix through the current settings: READY FOR EXPORT / PEAKS TOO HIGH / LOW-END COMPATIBILITY ISSUE / EXCESSIVE DENSITY / CHECK MIX. |

## Signal chain
`INPUT GAIN → DC FILTER → SUBSONIC → MASTER EQ (8 bands + TONE tilt) → DYNAMIC EQ (4 bands) → MULTIBAND (5 bands) → GLUE → PUNCH →
[oversampled] HARMONIC COLOR → STEREO / M-S → POLOOYX CHARACTER → [oversampled] SOFT CLIPPER → TRUE-PEAK LIMITER → OUTPUT → DRY/WET`
then (monitoring only) `A/B · BYPASS · MONO / SMALL-SPEAKER`.

Every module is **bit-transparent at its neutral setting** (verified by null tests), latency is constant (13.5 ms at 48 kHz) whatever the
oversampling or lookahead, and the output is bit-identical at any host buffer size.

## Six macros
| Macro | What it moves |
|---|---|
| **TONE** | ±2 dB see-saw tilt (250 Hz low shelf against 1.7 kHz high shelf) — never lifts both ends. |
| **GLUE** | Glue threshold −14 dB, ratio +2, makeup +3 dB at 100 %. |
| **COLOR** | Harmonic color mix +60 %, drive +35 % (mode set in COLOR page). |
| **WIDTH** | + : mid/high side gain up to +35 / +45 %, automatically held back when the live input correlation is low (< 0.5 → less, ≤ 0.1 → none); the low band is never widened. − : narrows all bands to 50 %. |
| **PUNCH** | Transient +60 %, low-band impact +50 %, body +15 %. |
| **LOUDNESS** | Clipper drive +3 dB and +9 dB into the true-peak limiter. The ceiling never moves — it is not an output-gain knob. |

## Views
* **BEGINNER** — analyser, six macros, diagnosis / next steps / result.
* **PRO** — every module: EQ (with curve), DYN EQ, MULTIBAND, GLUE / PUNCH, COLOR / CHARACTER, STEREO, CLIP / LIMIT, I/O / QUALITY.
* **LAB** — PRO + MEASURE: all live meters, per-stage gain reduction, latency, CPU, and the full last-analysis table.

## Documentation
| | |
|---|---|
| [docs/DSP_NOTES.md](docs/DSP_NOTES.md) | architecture, DSP decisions, analysis methodology, every POLO IT rule and threshold |
| [PARAMETERS.md (builds branch)](../../blob/builds/PARAMETERS.md) | all 170 automatable parameters — regenerated from the code by every CI build |
| [docs/PRESETS.md](docs/PRESETS.md) | factory presets, saving your own |
| [docs/LIMITATIONS.md](docs/LIMITATIONS.md) | known limitations of milestone 1 |
| [TEST_REPORT-Windows.md (builds branch)](../../blob/builds/TEST_REPORT-Windows.md) | 254 automated checks run on the Windows build: EBU 3341/3342 loudness, true peak, EQ, dynamics, M/S, POLO IT safety, A/B, clicks, sample rates, CPU |

## Formats / sample rates
VST3 (64-bit) + Standalone on Windows. 44.1, 48, 88.2, 96, 176.4 and 192 kHz (all verified in the test suite). Mono-in / stereo-out
and stereo-in / stereo-out. AU / macOS: not built in milestone 1 (the code is portable JUCE; a macOS CI job can be added).

## Building from source (optional — CI does this)
```bash
npm pack inter-ui@4.1.1 && tar xzf inter-ui-4.1.1.tgz && pip install fonttools brotli && python tools/make_fonts.py package/web Resources   # optional
cmake -B build -DCMAKE_BUILD_TYPE=Release            # Windows: -A x64 ; JUCE 8.0.4 is fetched automatically
cmake --build build --config Release --target PolooyxRelease_VST3 PolooyxRelease_Standalone PolooyxReleaseTests
./build/PolooyxReleaseTests_artefacts/Release/PolooyxReleaseTests        # writes test-output/TEST_REPORT.md (add --ui for snapshots)
```

## Layout
```
Source/
  PluginProcessor.*      capture, meters, background jobs, A/B + level match, presets, state
  PluginEditor.*         UI (1280x800 design, scales 960x600 … 2560x1600)
  Params.h  Presets.h    parameter layout, factory presets
  DSP/                   GainStage, MasterEQ, DynamicEQ, Multiband, GlueCompressor, Punch, Saturation,
                         StereoEngine, Character, Clipper, TruePeakLimiter, MasterChain, ChainSettings (macros)
  Analysis/              LoudnessAnalyzer (BS.1770), PeakAnalyzer (true peak), SpectrumAnalyzer, StereoAnalyzer, MixDiagnostics
  Intelligence/          RecommendationEngine, CorrectionModel, SafetyEngine, AutoMasterEngine (POLO IT / FINAL CHECK)
  UI/                    Look (grayscale), Widgets, AnalyzerViews, Panels, Pages, PresetBrowser
Tests/TestMain.cpp       verification harness (runs in CI on the Windows build; --ui renders UI snapshots)
tools/make_fonts.py      builds the Inter typeface resources (SIL OFL) from the inter-ui npm package - CI runs it
```
JUCE 8.0.4 is downloaded by CMake at configure time (AGPLv3 / JUCE licence).
