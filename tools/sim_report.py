#!/usr/bin/env python3
"""Turn simulator output (PGM frames + CSV log) into annotated PNGs and a
contact sheet per device. The red box marks the rect pushed to the panel."""
import csv
import glob
import os
import sys

from PIL import Image, ImageDraw, ImageFont

COLORS = {"FAST": (0, 140, 255), "QUALITY": (230, 60, 30), "FULL": (150, 0, 200)}
FONT = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 22)
SMALL = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 18)


def annotate(img, row):
    rgb = img.convert("RGB")
    d = ImageDraw.Draw(rgb)
    x, y, w, h = (int(row[k]) for k in ("x", "y", "w", "h"))
    color = COLORS.get(row["mode"], (255, 0, 0))
    for i in range(3):
        d.rectangle([x + i, y + i, x + w - 1 - i, y + h - 1 - i], outline=color)
    return rgb, color


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else "out"
    for log in sorted(glob.glob(os.path.join(out, "*_log.csv"))):
        name = os.path.basename(log)[:-8]
        rows = list(csv.DictReader(open(log)))
        thumbs = []
        for row in rows:
            pgm = os.path.join(out, f"{name}_{int(row['frame']):03d}.pgm")
            rgb, color = annotate(Image.open(pgm), row)
            png = pgm[:-4] + ".png"
            rgb.save(png)
            os.remove(pgm)
            thumbs.append((rgb, row, color))

        tw = 270
        th = int(thumbs[0][0].height * tw / thumbs[0][0].width)
        cols = 6
        cap = 64
        rows_n = (len(thumbs) + cols - 1) // cols
        sheet = Image.new("RGB", (cols * (tw + 16) + 16, rows_n * (th + cap + 16) + 16), (235, 235, 235))
        d = ImageDraw.Draw(sheet)
        for i, (rgb, row, color) in enumerate(thumbs):
            cx = 16 + (i % cols) * (tw + 16)
            cy = 16 + (i // cols) * (th + cap + 16)
            sheet.paste(rgb.resize((tw, th), Image.LANCZOS), (cx, cy))
            d.rectangle([cx - 1, cy - 1, cx + tw, cy + th], outline=(120, 120, 120))
            d.text((cx, cy + th + 4), f"#{row['frame']} {row['mode']}", fill=color, font=FONT)
            d.text((cx, cy + th + 32), row["step"][:30], fill=(30, 30, 30), font=SMALL)
        sheet_path = os.path.join(out, f"{name}_sheet.png")
        sheet.save(sheet_path)
        print(f"{name}: {len(rows)} frames -> {sheet_path}")


if __name__ == "__main__":
    main()
