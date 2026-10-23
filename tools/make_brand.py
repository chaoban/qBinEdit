#!/usr/bin/env python3
"""Build the SAISAI logo variants used by the About dialog from the master artwork
(resources/brand/saisai_logo_src.png: charcoal "SAIS" + orange "AI" on a light background).

Outputs (transparent background, trimmed, max 1200 px wide):
   saisai_logo_light.png  charcoal + orange  (for light banners)
   saisai_logo_dark.png   white    + orange  (for dark banners)

Every pixel is modelled as the background blended with one ink colour; the blend factor
becomes the alpha channel, so anti-aliased edges stay smooth on any banner colour.
Anything that is not clearly ink (e.g. a page number in a corner) is ignored when trimming.
Usage: python3 tools/make_brand.py [repo-root]
"""
import os
import sys

import numpy as np
from PIL import Image

ROOT = sys.argv[1] if len(sys.argv) > 1 else os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BRAND = os.path.join(ROOT, "resources", "brand")
MAX_W = 1200

src = np.asarray(Image.open(os.path.join(BRAND, "saisai_logo_src.png")).convert("RGB")).astype(np.float64)
h, w, _ = src.shape
border = np.concatenate([src[0], src[-1], src[:, 0], src[:, -1]])
bg = np.median(border, axis=0)

lum = src.mean(axis=2)
dark_mask = lum < 90
orange_mask = (src[..., 0] > 190) & (src[..., 1] > 80) & (src[..., 1] < 170) & (src[..., 2] < 100)
charcoal = src[dark_mask].mean(axis=0)
orange = src[orange_mask].mean(axis=0)

# trim to the ink (dark + orange) with a small margin
ys, xs = np.where(dark_mask | orange_mask)
m = 6
y0, y1 = max(0, ys.min() - m), min(h, ys.max() + m + 1)
x0, x1 = max(0, xs.min() - m), min(w, xs.max() + m + 1)
crop = Image.fromarray(src[y0:y1, x0:x1].astype(np.uint8))
if crop.width > MAX_W:
    crop = crop.resize((MAX_W, round(crop.height * MAX_W / crop.width)), Image.LANCZOS)
p = np.asarray(crop).astype(np.float64)

def blend(ink):
    d = ink - bg
    a = np.clip(((p - bg) * d).sum(axis=2) / (d * d).sum(), 0.0, 1.0)
    resid = ((p - bg - a[..., None] * d) ** 2).sum(axis=2)
    return a, resid

a_c, r_c = blend(charcoal)
a_o, r_o = blend(orange)
is_orange = r_o < r_c
alpha = np.where(is_orange, a_o, a_c)
alpha = np.where(alpha < 0.05, 0.0, alpha)          # drop background noise

variants = {
    "light": (charcoal, orange),
    "dark": (np.array([255.0, 255.0, 255.0]), orange),
}
for name, (c_ink, o_ink) in variants.items():
    rgb = np.where(is_orange[..., None], o_ink, c_ink)
    out = np.dstack([rgb, alpha * 255.0]).round().clip(0, 255).astype(np.uint8)
    Image.fromarray(out, "RGBA").save(os.path.join(BRAND, f"saisai_logo_{name}.png"), optimize=True)
    print(name, out.shape[1], "x", out.shape[0])
print("background", bg.round(), "charcoal", charcoal.round(), "orange", orange.round())
