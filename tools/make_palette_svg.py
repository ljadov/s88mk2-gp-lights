#!/usr/bin/env python3
"""Genere docs/palette.svg (charte de couleurs) a partir de shared/Palette.h."""
import re, sys, pathlib

root = pathlib.Path(__file__).resolve().parent.parent
src = (root / "shared" / "Palette.h").read_text()
hues = [(int(i), fr, en, int(rgb, 16)) for i, fr, en, rgb in
        re.findall(r'\{(\d+), "([^"]+)", "([^"]+)", 0x([0-9A-Fa-f]{6})\}', src)]
labels = {"dore": "doré"}
shades = [("clair", "light", 3, 1.0), ("vif", "bright", 2, 0.8), ("moyen", "medium", 1, 0.55), ("sombre", "dark", 0, 0.35)]

cw, ch, x0, y0 = 92, 44, 120, 64
w = x0 + cw * 4 + 24
h = y0 + ch * len(hues) + 24
out = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{w}" height="{h}" viewBox="0 0 {w} {h}" font-family="Segoe UI, Helvetica, Arial, sans-serif">',
       f'<rect width="{w}" height="{h}" fill="#1e1f24"/>',
       '<text x="16" y="28" font-size="16" font-weight="600" fill="#e8e8ea">Charte de couleurs Kontrol Lights (S88 MK2)</text>']
for c, (fr, en, s, _) in enumerate(shades):
    cx = x0 + c * cw + cw / 2
    out.append(f'<text x="{cx}" y="54" font-size="12" text-anchor="middle" fill="#9a9ca3">{fr} / {en}</text>')
for r, (i, fr, en, rgb) in enumerate(hues):
    y = y0 + r * ch
    out.append(f'<text x="16" y="{y + 20}" font-size="13" font-weight="600" fill="#e8e8ea">{labels.get(fr, fr)}</text>')
    out.append(f'<text x="16" y="{y + 36}" font-size="11" fill="#9a9ca3">{en}</text>')
    for c, (_, _, s, b) in enumerate(shades):
        rr, gg, bb = [int(((rgb >> k) & 255) * b) for k in (16, 8, 0)]
        idx = i * 4 + s
        txt = "#111" if (rr * 299 + gg * 587 + bb * 114) / 1000 > 140 else "#f0f0f0"
        x = x0 + c * cw
        out.append(f'<rect x="{x + 4}" y="{y + 4}" width="{cw - 8}" height="{ch - 8}" rx="6" fill="#{rr:02x}{gg:02x}{bb:02x}"/>')
        out.append(f'<text x="{x + cw / 2}" y="{y + ch / 2 + 5}" font-size="13" text-anchor="middle" fill="{txt}">{idx}</text>')
out.append('</svg>')
(root / "docs" / "palette.svg").write_text("\n".join(out), encoding="utf-8")
print("docs/palette.svg :", len(hues), "teintes")
