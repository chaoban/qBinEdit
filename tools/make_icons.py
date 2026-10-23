#!/usr/bin/env python3
"""Generate qBinEdit app icon (PNG/ICO) and MSIX tile assets with Pillow.

Usage: python3 tools/make_icons.py <repo-root>
"""
import os, sys
from PIL import Image, ImageDraw, ImageFont

ROOT = sys.argv[1] if len(sys.argv) > 1 else os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FONT_CANDIDATES = [
    "/usr/share/fonts/truetype/dejavu/DejaVuSansMono-Bold.ttf",
    "C:/Windows/Fonts/consolab.ttf",
]

def mono_font(size):
    for f in FONT_CANDIDATES:
        if os.path.exists(f):
            return ImageFont.truetype(f, size)
    return ImageFont.load_default()

def glyph(size, full_bleed=False):
    """Master square icon: chip outline with '0x' and bit rows on a blue rounded square."""
    S = 1024
    img = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    # background: vertical gradient rounded square
    top, bot = (37, 99, 235), (30, 58, 138)
    grad = Image.new("RGBA", (S, S))
    gd = ImageDraw.Draw(grad)
    for y in range(S):
        t = y / (S - 1)
        c = tuple(int(top[i] * (1 - t) + bot[i] * t) for i in range(3)) + (255,)
        gd.line([(0, y), (S, y)], fill=c)
    mask = Image.new("L", (S, S), 0)
    m = 0 if full_bleed else 40
    ImageDraw.Draw(mask).rounded_rectangle([m, m, S - m, S - m], radius=0 if full_bleed else 210, fill=255)
    img.paste(grad, (0, 0), mask)
    # chip body
    cx0, cy0, cx1, cy1 = 250, 250, 774, 774
    pin_w, pin_l, gap = 44, 80, 104
    for i in range(4):
        p = cx0 + 70 + i * gap
        d.rounded_rectangle([p, cy0 - pin_l, p + pin_w, cy0 + 10], radius=12, fill=(191, 219, 254, 255))
        d.rounded_rectangle([p, cy1 - 10, p + pin_w, cy1 + pin_l], radius=12, fill=(191, 219, 254, 255))
        q = cy0 + 70 + i * gap
        d.rounded_rectangle([cx0 - pin_l, q, cx0 + 10, q + pin_w], radius=12, fill=(191, 219, 254, 255))
        d.rounded_rectangle([cx1 - 10, q, cx1 + pin_l, q + pin_w], radius=12, fill=(191, 219, 254, 255))
    d.rounded_rectangle([cx0, cy0, cx1, cy1], radius=60, fill=(15, 23, 42, 255), outline=(147, 197, 253, 255), width=14)
    # text
    f = mono_font(250)
    txt = "0x"
    bb = d.textbbox((0, 0), txt, font=f)
    w, h = bb[2] - bb[0], bb[3] - bb[1]
    d.text(((S - w) / 2 - bb[0], 360 - bb[1]), txt, font=f, fill=(255, 255, 255, 255))
    f2 = mono_font(96)
    bits = "1011"
    bb = d.textbbox((0, 0), bits, font=f2)
    w = bb[2] - bb[0]
    d.text(((S - w) / 2 - bb[0], 620 - bb[1]), bits, font=f2, fill=(96, 165, 250, 255))
    return img.resize((size, size), Image.LANCZOS)

def save(img, path):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    img.save(path)

icons = os.path.join(ROOT, "resources", "icons")
master = glyph(1024)
save(master.resize((256, 256), Image.LANCZOS), os.path.join(icons, "qbinedit.png"))
master.save(os.path.join(icons, "qbinedit.ico"), sizes=[(16, 16), (20, 20), (24, 24), (32, 32), (40, 40), (48, 48), (64, 64), (128, 128), (256, 256)])
save(master.resize((300, 300), Image.LANCZOS), os.path.join(ROOT, "packaging", "store", "StoreLogo_300x300.png"))

# ---- MSIX visual assets (with scale / targetsize qualifiers; resources.pri is built by makepri)
assets = os.path.join(ROOT, "packaging", "msix", "Assets")

def square(name, base, scales=(100, 125, 150, 200, 400), pad=0.0):
    for sc in scales:
        px = round(base * sc / 100)
        canvas = Image.new("RGBA", (px, px), (0, 0, 0, 0))
        inner = round(px * (1 - 2 * pad))
        g = master.resize((inner, inner), Image.LANCZOS)
        canvas.paste(g, ((px - inner) // 2, (px - inner) // 2), g)
        save(canvas, os.path.join(assets, f"{name}.scale-{sc}.png"))

def wide(name, bw, bh, scales=(100, 125, 150, 200, 400), title=False):
    for sc in scales:
        W, H = round(bw * sc / 100), round(bh * sc / 100)
        canvas = Image.new("RGBA", (W, H), (0, 0, 0, 0))
        side = round(H * 0.66)
        g = master.resize((side, side), Image.LANCZOS)
        if title:
            x = round(W * 0.5 - side * 1.1)
            canvas.paste(g, (x, (H - side) // 2), g)
            f = mono_font(round(H * 0.20))
            dd = ImageDraw.Draw(canvas)
            dd.text((x + side + round(H * 0.08), H // 2), "qBinEdit", font=f, fill=(255, 255, 255, 255), anchor="lm")
        else:
            canvas.paste(g, ((W - side) // 2, (H - side) // 2), g)
        save(canvas, os.path.join(assets, f"{name}.scale-{sc}.png"))

square("Square44x44Logo", 44)
square("Square150x150Logo", 150, pad=0.12)
square("SmallTile", 71, pad=0.10)
square("LargeTile", 310, pad=0.15)
square("StoreLogo", 50)
wide("Wide310x150Logo", 310, 150)
wide("SplashScreen", 620, 300, title=True)
# taskbar / start-menu list icons (targetsize, plated + unplated)
for ts in (16, 20, 24, 30, 32, 36, 40, 48, 60, 64, 72, 80, 96, 256):
    g = master.resize((ts, ts), Image.LANCZOS)
    save(g, os.path.join(assets, f"Square44x44Logo.targetsize-{ts}.png"))
    save(g, os.path.join(assets, f"Square44x44Logo.targetsize-{ts}_altform-unplated.png"))
    save(g, os.path.join(assets, f"Square44x44Logo.altform-lightunplated_targetsize-{ts}.png"))
print("icons written")
