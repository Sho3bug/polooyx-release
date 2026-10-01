# POLOOYX RELEASE ENGINE - Parameter Reference

Generated from the plugin's own parameter layout (`PolooyxReleaseTests --list-params`), so it always matches the build.
Every parameter is automatable in FL Studio (right-click a control -> *Create automation clip*, or *Browse parameters*).

| # | Name (as the host shows it) | ID | Range | Default |
|---|---|---|---|---|
| 1 | Input Gain | `inGain` | -24.0 dB ... +24.0 dB | 0.0 dB |
| 2 | Output Gain | `outGain` | -24.0 dB ... +12.0 dB | 0.0 dB |
| 3 | Dry/Wet | `dryWet` | 0% ... 100% | 100% |
| 4 | Bypass | `bypass` | Off / On | Off |
| 5 | Listen | `listen` | PROCESSED / ORIGINAL | PROCESSED |
| 6 | Level Match | `levelMatch` | Off / On | On |
| 7 | Monitor | `monitor` | FULL / MONO / SMALL SPEAKER | FULL |
| 8 | Quality | `quality` | ECO / NORMAL / HIGH / MAX | NORMAL |
| 9 | Oversampling | `oversampling` | AUTO / OFF / 2X / 4X / 8X | AUTO |
| 10 | Loudness Target | `target` | MIX / PREMASTER / STREAMING / LOUD / CUSTOM | STREAMING |
| 11 | Custom Target | `customTarget` | -24.0 LUFS ... -5.0 LUFS | -12.0 LUFS |
| 12 | POLO IT Mode | `poloMode` | TRANSPARENT / BALANCED / CHARACTER / LOUD / SAFE | BALANCED |
| 13 | TONE | `tone` | -100% ... +100% | 0% |
| 14 | GLUE | `glue` | 0% ... 100% | 0% |
| 15 | COLOR | `color` | 0% ... 100% | 0% |
| 16 | WIDTH | `width` | -100% ... +100% | 0% |
| 17 | PUNCH | `punch` | 0% ... 100% | 0% |
| 18 | LOUDNESS | `loudness` | 0% ... 100% | 0% |
| 19 | DC Filter | `dcOn` | Off / On | Off |
| 20 | Subsonic Filter | `subOn` | Off / On | Off |
| 21 | Subsonic Freq | `subFreq` | 10 Hz ... 40 Hz | 18 Hz |
| 22 | EQ On | `eqOn` | Off / On | On |
| 23 | EQ SUB Type | `eq1Type` | BELL / LOW SHELF / HIGH SHELF / HIGH-PASS / LOW-PASS | BELL |
| 24 | EQ SUB Freq | `eq1Freq` | 20 Hz ... 20.0 kHz | 45 Hz |
| 25 | EQ SUB Gain | `eq1Gain` | -12.0 dB ... +12.0 dB | 0.0 dB |
| 26 | EQ SUB Q | `eq1Q` | 0.20 ... 6.00 | 0.80 |
| 27 | EQ LOW Type | `eq2Type` | BELL / LOW SHELF / HIGH SHELF / HIGH-PASS / LOW-PASS | LOW SHELF |
| 28 | EQ LOW Freq | `eq2Freq` | 20 Hz ... 20.0 kHz | 110 Hz |
| 29 | EQ LOW Gain | `eq2Gain` | -12.0 dB ... +12.0 dB | 0.0 dB |
| 30 | EQ LOW Q | `eq2Q` | 0.20 ... 6.00 | 0.70 |
| 31 | EQ LOW-MID Type | `eq3Type` | BELL / LOW SHELF / HIGH SHELF / HIGH-PASS / LOW-PASS | BELL |
| 32 | EQ LOW-MID Freq | `eq3Freq` | 20 Hz ... 20.0 kHz | 250 Hz |
| 33 | EQ LOW-MID Gain | `eq3Gain` | -12.0 dB ... +12.0 dB | 0.0 dB |
| 34 | EQ LOW-MID Q | `eq3Q` | 0.20 ... 6.00 | 1.00 |
| 35 | EQ MID Type | `eq4Type` | BELL / LOW SHELF / HIGH SHELF / HIGH-PASS / LOW-PASS | BELL |
| 36 | EQ MID Freq | `eq4Freq` | 20 Hz ... 20.0 kHz | 700 Hz |
| 37 | EQ MID Gain | `eq4Gain` | -12.0 dB ... +12.0 dB | 0.0 dB |
| 38 | EQ MID Q | `eq4Q` | 0.20 ... 6.00 | 1.00 |
| 39 | EQ UPPER-MID Type | `eq5Type` | BELL / LOW SHELF / HIGH SHELF / HIGH-PASS / LOW-PASS | BELL |
| 40 | EQ UPPER-MID Freq | `eq5Freq` | 20 Hz ... 20.0 kHz | 1.80 kHz |
| 41 | EQ UPPER-MID Gain | `eq5Gain` | -12.0 dB ... +12.0 dB | 0.0 dB |
| 42 | EQ UPPER-MID Q | `eq5Q` | 0.20 ... 6.00 | 1.00 |
| 43 | EQ PRESENCE Type | `eq6Type` | BELL / LOW SHELF / HIGH SHELF / HIGH-PASS / LOW-PASS | BELL |
| 44 | EQ PRESENCE Freq | `eq6Freq` | 20 Hz ... 20.0 kHz | 4.00 kHz |
| 45 | EQ PRESENCE Gain | `eq6Gain` | -12.0 dB ... +12.0 dB | 0.0 dB |
| 46 | EQ PRESENCE Q | `eq6Q` | 0.20 ... 6.00 | 1.00 |
| 47 | EQ HIGH Type | `eq7Type` | BELL / LOW SHELF / HIGH SHELF / HIGH-PASS / LOW-PASS | HIGH SHELF |
| 48 | EQ HIGH Freq | `eq7Freq` | 20 Hz ... 20.0 kHz | 9.00 kHz |
| 49 | EQ HIGH Gain | `eq7Gain` | -12.0 dB ... +12.0 dB | 0.0 dB |
| 50 | EQ HIGH Q | `eq7Q` | 0.20 ... 6.00 | 0.70 |
| 51 | EQ AIR Type | `eq8Type` | BELL / LOW SHELF / HIGH SHELF / HIGH-PASS / LOW-PASS | HIGH SHELF |
| 52 | EQ AIR Freq | `eq8Freq` | 20 Hz ... 20.0 kHz | 16.0 kHz |
| 53 | EQ AIR Gain | `eq8Gain` | -12.0 dB ... +12.0 dB | 0.0 dB |
| 54 | EQ AIR Q | `eq8Q` | 0.20 ... 6.00 | 0.70 |
| 55 | Dynamic EQ On | `deqOn` | Off / On | On |
| 56 | DynEQ LOW-MID Freq | `deq1Freq` | 60 Hz ... 16.0 kHz | 300 Hz |
| 57 | DynEQ LOW-MID Q | `deq1Q` | 0.30 ... 6.00 | 1.20 |
| 58 | DynEQ LOW-MID Threshold | `deq1Thr` | -60.0 dB ... 0.0 dB | 0.0 dB |
| 59 | DynEQ LOW-MID Ratio | `deq1Ratio` | 1.0:1 ... 10.0:1 | 2.0:1 |
| 60 | DynEQ LOW-MID Attack | `deq1Att` | 0.5 ms ... 100 ms | 5.0 ms |
| 61 | DynEQ LOW-MID Release | `deq1Rel` | 10 ms ... 1000 ms | 120 ms |
| 62 | DynEQ LOW-MID Range | `deq1Range` | 0.0 dB ... +12.0 dB | +3.0 dB |
| 63 | DynEQ LOW-MID Mix | `deq1Mix` | 0% ... 100% | 100% |
| 64 | DynEQ MID Freq | `deq2Freq` | 60 Hz ... 16.0 kHz | 900 Hz |
| 65 | DynEQ MID Q | `deq2Q` | 0.30 ... 6.00 | 1.20 |
| 66 | DynEQ MID Threshold | `deq2Thr` | -60.0 dB ... 0.0 dB | 0.0 dB |
| 67 | DynEQ MID Ratio | `deq2Ratio` | 1.0:1 ... 10.0:1 | 2.0:1 |
| 68 | DynEQ MID Attack | `deq2Att` | 0.5 ms ... 100 ms | 5.0 ms |
| 69 | DynEQ MID Release | `deq2Rel` | 10 ms ... 1000 ms | 120 ms |
| 70 | DynEQ MID Range | `deq2Range` | 0.0 dB ... +12.0 dB | +3.0 dB |
| 71 | DynEQ MID Mix | `deq2Mix` | 0% ... 100% | 100% |
| 72 | DynEQ PRESENCE Freq | `deq3Freq` | 60 Hz ... 16.0 kHz | 3.50 kHz |
| 73 | DynEQ PRESENCE Q | `deq3Q` | 0.30 ... 6.00 | 1.20 |
| 74 | DynEQ PRESENCE Threshold | `deq3Thr` | -60.0 dB ... 0.0 dB | 0.0 dB |
| 75 | DynEQ PRESENCE Ratio | `deq3Ratio` | 1.0:1 ... 10.0:1 | 2.0:1 |
| 76 | DynEQ PRESENCE Attack | `deq3Att` | 0.5 ms ... 100 ms | 5.0 ms |
| 77 | DynEQ PRESENCE Release | `deq3Rel` | 10 ms ... 1000 ms | 120 ms |
| 78 | DynEQ PRESENCE Range | `deq3Range` | 0.0 dB ... +12.0 dB | +3.0 dB |
| 79 | DynEQ PRESENCE Mix | `deq3Mix` | 0% ... 100% | 100% |
| 80 | DynEQ HIGH Freq | `deq4Freq` | 60 Hz ... 16.0 kHz | 8.00 kHz |
| 81 | DynEQ HIGH Q | `deq4Q` | 0.30 ... 6.00 | 1.20 |
| 82 | DynEQ HIGH Threshold | `deq4Thr` | -60.0 dB ... 0.0 dB | 0.0 dB |
| 83 | DynEQ HIGH Ratio | `deq4Ratio` | 1.0:1 ... 10.0:1 | 2.0:1 |
| 84 | DynEQ HIGH Attack | `deq4Att` | 0.5 ms ... 100 ms | 5.0 ms |
| 85 | DynEQ HIGH Release | `deq4Rel` | 10 ms ... 1000 ms | 120 ms |
| 86 | DynEQ HIGH Range | `deq4Range` | 0.0 dB ... +12.0 dB | +3.0 dB |
| 87 | DynEQ HIGH Mix | `deq4Mix` | 0% ... 100% | 100% |
| 88 | Multiband On | `mbOn` | Off / On | On |
| 89 | Crossover 1 | `xo1` | 40 Hz ... 200 Hz | 90 Hz |
| 90 | Crossover 2 | `xo2` | 150 Hz ... 800 Hz | 300 Hz |
| 91 | Crossover 3 | `xo3` | 800 Hz ... 5.00 kHz | 2.50 kHz |
| 92 | Crossover 4 | `xo4` | 4.00 kHz ... 14.0 kHz | 8.00 kHz |
| 93 | MB SUB Threshold | `mb1Thr` | -60.0 dB ... 0.0 dB | 0.0 dB |
| 94 | MB SUB Ratio | `mb1Ratio` | 1.0:1 ... 10.0:1 | 2.0:1 |
| 95 | MB SUB Attack | `mb1Att` | 0.1 ms ... 200 ms | 20 ms |
| 96 | MB SUB Release | `mb1Rel` | 10 ms ... 1000 ms | 150 ms |
| 97 | MB SUB Makeup | `mb1Gain` | -12.0 dB ... +12.0 dB | 0.0 dB |
| 98 | MB SUB Mix | `mb1Mix` | 0% ... 100% | 100% |
| 99 | MB LOW Threshold | `mb2Thr` | -60.0 dB ... 0.0 dB | 0.0 dB |
| 100 | MB LOW Ratio | `mb2Ratio` | 1.0:1 ... 10.0:1 | 2.0:1 |
| 101 | MB LOW Attack | `mb2Att` | 0.1 ms ... 200 ms | 20 ms |
| 102 | MB LOW Release | `mb2Rel` | 10 ms ... 1000 ms | 150 ms |
| 103 | MB LOW Makeup | `mb2Gain` | -12.0 dB ... +12.0 dB | 0.0 dB |
| 104 | MB LOW Mix | `mb2Mix` | 0% ... 100% | 100% |
| 105 | MB LOW-MID Threshold | `mb3Thr` | -60.0 dB ... 0.0 dB | 0.0 dB |
| 106 | MB LOW-MID Ratio | `mb3Ratio` | 1.0:1 ... 10.0:1 | 2.0:1 |
| 107 | MB LOW-MID Attack | `mb3Att` | 0.1 ms ... 200 ms | 20 ms |
| 108 | MB LOW-MID Release | `mb3Rel` | 10 ms ... 1000 ms | 150 ms |
| 109 | MB LOW-MID Makeup | `mb3Gain` | -12.0 dB ... +12.0 dB | 0.0 dB |
| 110 | MB LOW-MID Mix | `mb3Mix` | 0% ... 100% | 100% |
| 111 | MB HIGH-MID Threshold | `mb4Thr` | -60.0 dB ... 0.0 dB | 0.0 dB |
| 112 | MB HIGH-MID Ratio | `mb4Ratio` | 1.0:1 ... 10.0:1 | 2.0:1 |
| 113 | MB HIGH-MID Attack | `mb4Att` | 0.1 ms ... 200 ms | 20 ms |
| 114 | MB HIGH-MID Release | `mb4Rel` | 10 ms ... 1000 ms | 150 ms |
| 115 | MB HIGH-MID Makeup | `mb4Gain` | -12.0 dB ... +12.0 dB | 0.0 dB |
| 116 | MB HIGH-MID Mix | `mb4Mix` | 0% ... 100% | 100% |
| 117 | MB HIGH Threshold | `mb5Thr` | -60.0 dB ... 0.0 dB | 0.0 dB |
| 118 | MB HIGH Ratio | `mb5Ratio` | 1.0:1 ... 10.0:1 | 2.0:1 |
| 119 | MB HIGH Attack | `mb5Att` | 0.1 ms ... 200 ms | 20 ms |
| 120 | MB HIGH Release | `mb5Rel` | 10 ms ... 1000 ms | 150 ms |
| 121 | MB HIGH Makeup | `mb5Gain` | -12.0 dB ... +12.0 dB | 0.0 dB |
| 122 | MB HIGH Mix | `mb5Mix` | 0% ... 100% | 100% |
| 123 | Multiband Auto Gain | `mbAutoGain` | Off / On | Off |
| 124 | Glue On | `glueOn` | Off / On | On |
| 125 | Glue Threshold | `glueThr` | -40.0 dB ... 0.0 dB | 0.0 dB |
| 126 | Glue Ratio | `glueRatio` | 1.0:1 ... 10.0:1 | 2.0:1 |
| 127 | Glue Attack | `glueAtt` | 0.1 ms ... 100 ms | 30 ms |
| 128 | Glue Release | `glueRel` | 20 ms ... 1200 ms | 200 ms |
| 129 | Glue Knee | `glueKnee` | 0.0 dB ... +12.0 dB | +6.0 dB |
| 130 | Glue Makeup | `glueMakeup` | 0.0 dB ... +12.0 dB | 0.0 dB |
| 131 | Glue Mix | `glueMix` | 0% ... 100% | 100% |
| 132 | Punch On | `punchOn` | Off / On | On |
| 133 | Punch Transient | `punchTrans` | -100% ... +100% | 0% |
| 134 | Punch Body | `punchBody` | -100% ... +100% | 0% |
| 135 | Punch Impact | `punchImpact` | 0% ... 100% | 0% |
| 136 | Punch Mix | `punchMix` | 0% ... 100% | 100% |
| 137 | Color On | `colorOn` | Off / On | On |
| 138 | Color Mode | `colorMode` | CLEAN / TAPE / TUBE / CONSOLE / DIGITAL | TAPE |
| 139 | Color Drive | `colorDrive` | 0% ... 100% | 30% |
| 140 | Color Tone | `colorTone` | -100% ... +100% | 0% |
| 141 | Color Mix | `colorMix` | 0% ... 100% | 0% |
| 142 | Color Output | `colorOut` | -12.0 dB ... +12.0 dB | 0.0 dB |
| 143 | Stereo On | `stOn` | Off / On | On |
| 144 | Width | `stWidth` | 0% ... 200% | 100% |
| 145 | Low Width | `stLowWidth` | 0% ... 200% | 100% |
| 146 | High Width | `stHighWidth` | 0% ... 200% | 100% |
| 147 | Low Width Xover | `stXoLow` | 100 Hz ... 800 Hz | 250 Hz |
| 148 | High Width Xover | `stXoHigh` | 1.50 kHz ... 12.0 kHz | 5.00 kHz |
| 149 | Mid Level | `stMid` | -6.0 dB ... +6.0 dB | 0.0 dB |
| 150 | Side Level | `stSide` | -6.0 dB ... +6.0 dB | 0.0 dB |
| 151 | Low Frequency Mono | `monoBass` | Off / On | Off |
| 152 | Mono Below | `monoFreq` | 40 Hz ... 300 Hz | 80 Hz |
| 153 | Character On | `charOn` | Off / On | On |
| 154 | Aura | `aura` | 0% ... 100% | 0% |
| 155 | Grit | `grit` | 0% ... 100% | 0% |
| 156 | Glass | `glass` | 0% ... 100% | 0% |
| 157 | Depth | `depth` | 0% ... 100% | 0% |
| 158 | Space | `space` | 0% ... 100% | 0% |
| 159 | Clipper On | `clipOn` | Off / On | On |
| 160 | Clipper Mode | `clipMode` | SOFT / TAPE / HARD | SOFT |
| 161 | Clipper Drive | `clipDrive` | 0.0 dB ... +12.0 dB | 0.0 dB |
| 162 | Clipper Ceiling | `clipCeiling` | -6.0 dB ... 0.0 dB | -0.3 dB |
| 163 | Clipper Mix | `clipMix` | 0% ... 100% | 100% |
| 164 | Limiter On | `limOn` | Off / On | On |
| 165 | Limiter Style | `limStyle` | TRANSPARENT / PUNCH / DENSE / MODERN | TRANSPARENT |
| 166 | Limiter Threshold | `limThresh` | -24.0 dB ... 0.0 dB | 0.0 dB |
| 167 | Limiter Ceiling | `limCeiling` | -6.0 dBTP ... 0.0 dBTP | -1.0 dBTP |
| 168 | Limiter Release | `limRelease` | 1.0 ms ... 1000 ms | 120 ms |
| 169 | Limiter Lookahead | `limLookahead` | 0.5 ms ... 10 ms | 5.0 ms |
| 170 | Limiter Mix | `limMix` | 0% ... 100% | 100% |
