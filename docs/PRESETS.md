# POLOOYX RELEASE ENGINE — presets

## Factory presets (21)
Open the preset name in the top bar for the searchable browser, or step with **< / >**. They are also exposed to the host as VST3
programs. A factory preset first resets every DSP parameter to its default, then applies a few restrained moves. Session settings
(bypass, A/B listen, level match, monitor, quality, oversampling, loudness target, POLO IT mode) are left alone.

| Category | Preset | What it does |
|---|---|---|
| CORE | POLOOYX DEFAULT | neutral: only the true-peak limiter guarding −1 dBTP |
| | POLOOYX CLEAN | light glue, long lookahead / release limiter |
| | POLOOYX BALANCED | glue 20, punch 10, color 8, loudness 25 |
| | POLOOYX DARK | tone −35, tube color, high shelf −1 dB, glue |
| | POLOOYX POLISHED | glue, slight brightness, aura, glass, punch |
| RELEASE | POLOOYX RELEASE | glue, punch, color, loudness 35 |
| | POLOOYX STREAM | light glue, loudness 25, transparent limiter at −1 dBTP |
| | POLOOYX MODERN | glue, punch, loudness 45, MODERN limiter |
| | POLOOYX LOUD | glue 30, loudness 65, 1 dB clip, DENSE limiter |
| | POLOOYX TRANSPARENT | limiter only, 8 ms lookahead, 220 ms release |
| CHARACTER | POLOOYX AURA / GRIT / GLASS / DEPTH / SPACE | one character dimension each, with low-end mono where width is added |
| STYLE | UNDERGROUND | darker tilt, grit, tape color, punch, glue |
| | DARK | tone −45, tube color, depth |
| | PSYCHEDELIC | aura, space, depth, width (low end kept mono) |
| | HEAVY | glue, punch, grit, color, loudness 55, DENSE limiter |
| | WIDE | width 40, aura, low end mono below 100 Hz |
| | VOCAL FORWARD | +1 dB at 3.2 kHz, −0.8 dB at 300 Hz, light glue |

Every factory preset is rendered in the test suite and must stay under the ceiling.

## Your own presets
1. Dial in the sound (or let POLO IT do it), press **SAVE**, name it.
2. Saved to `Documents\POLOOYX\Release Presets\<name>.prelease` — a plain XML file of every parameter (including quality,
   oversampling and loudness target). Copy the folder to back presets up or share them.
3. They appear under **USER** in the browser (searchable).

## A / B settings slots
**A** and **B** in the top bar hold two complete settings. Switching stores the current one and recalls the other — compare two masters
quickly. (Different from **A / B COMPARE**, which compares ORIGINAL vs PROCESSED audio.)

## In FL Studio
The plugin's whole state (all parameters, view, analyser options, current preset name) is saved with the project.
