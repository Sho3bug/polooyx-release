#!/usr/bin/env python3
"""Builds Resources/Inter-*.ttf (subset, desktop TTF) from the open-source Inter web fonts.

Usage:  npm pack inter-ui@4.1.1 && tar xzf inter-ui-4.1.1.tgz && pip install fonttools brotli
        python tools/make_fonts.py package/web Resources
Inter is licensed under the SIL Open Font License 1.1 (copied next to the fonts).
If the fonts are missing, the plugin still builds and falls back to the system sans-serif.
"""
import os, shutil, sys
from fontTools.ttLib import TTFont
from fontTools import subset

src, dst = sys.argv[1], sys.argv[2]
os.makedirs(dst, exist_ok=True)
chars = list(range(0x20, 0x7F)) + [0xB0, 0xB1, 0xB7, 0xD7, 0x2013, 0x2014, 0x2022, 0x2026, 0x2190, 0x2191, 0x2192, 0x2193, 0x2212, 0x2264, 0x2265, 0x2248, 0x221E]
for w in ["Regular", "Medium", "SemiBold"]:
    f = TTFont(os.path.join(src, f"Inter-{w}.woff2"))
    f.flavor = None
    opts = subset.Options(); opts.layout_features = ["kern", "tnum", "case"]; opts.name_IDs = ["*"]; opts.notdef_outline = True
    s = subset.Subsetter(opts); s.populate(unicodes=chars); s.subset(f)
    f.save(os.path.join(dst, f"Inter-{w}.ttf"))
    print("wrote", os.path.join(dst, f"Inter-{w}.ttf"))
lic = os.path.join(os.path.dirname(os.path.normpath(src)), "LICENSE.txt")
if os.path.exists(lic): shutil.copy(lic, os.path.join(dst, "Inter-LICENSE.txt"))
