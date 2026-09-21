#!/usr/bin/env python3
"""Round the corners of a documentation screenshot PNG.

Fills pixels outside a rounded rectangle with near-black so squared
desktop/phone capture corners do not show in the PDF frame.

Usage:
  python3 mask_window_corners.py input.png [output.png] [--radius 28]
  python3 mask_window_corners.py --all   # every **/figures/*.png under repo root

Overwrites input when output is omitted.
"""
from __future__ import annotations

import argparse
import sys
from pathlib import Path

from PIL import Image, ImageDraw

FILL = (18, 18, 20)
ROOT = Path(__file__).resolve().parents[2]  # TeleCon_ESP32


def round_mask(src: Path, dst: Path, radius: int | None) -> None:
    im = Image.open(src).convert("RGBA")
    w, h = im.size
    r = radius if radius is not None else max(20, min(w, h) // 28)
    r = max(1, min(r, w // 2, h // 2))
    base = Image.new("RGBA", (w, h), FILL + (255,))
    mask = Image.new("L", (w, h), 0)
    ImageDraw.Draw(mask).rounded_rectangle((0, 0, w - 1, h - 1), radius=r, fill=255)
    out = Image.composite(im, base, mask).convert("RGB")
    dst.parent.mkdir(parents=True, exist_ok=True)
    out.save(dst, format="PNG", optimize=True)
    print(f"{src}: {w}x{h} r={r} -> {dst}")


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("input", type=Path, nargs="?")
    ap.add_argument("output", type=Path, nargs="?")
    ap.add_argument("--radius", type=int, default=None)
    ap.add_argument("--all", action="store_true", help="Process all figures/*.png")
    ap.add_argument("--root", type=Path, default=ROOT)
    args = ap.parse_args()
    if args.all:
        n = 0
        for p in sorted(args.root.rglob("figures/*.png")):
            if p.stat().st_size == 0:
                continue
            round_mask(p, p, args.radius)
            n += 1
        print(f"processed {n} files")
        return 0
    if not args.input or not args.input.is_file():
        print("missing input (or use --all)", file=sys.stderr)
        return 1
    round_mask(args.input, args.output or args.input, args.radius)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
