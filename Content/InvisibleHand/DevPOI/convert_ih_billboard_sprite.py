#!/usr/bin/env python3
"""Convert IH billboard source art (JPG/PNG) into runtime-ready PNG sprites.

Usage (from any shell):
  python D:\\Projects\\IH_P1C12_Arbor\\Content\\InvisibleHand\\DevPOI\\convert_ih_billboard_sprite.py

Optional args:
  python convert_ih_billboard_sprite.py <source.jpg> <output.png> [filled|line_overlay]

Modes:
  colorized    — preserve RGB from pre-colored art; key neutral checkerboard/white background
  filled       — grayscale body art for primary billboard fill (legacy tint pipeline)
  line_overlay — white ink + alpha for secondary outline/detail overlay

Source art defaults live in D:\\InvisibleHandCharts\\
Outputs land in this script's directory (Content/InvisibleHand/DevPOI/).
"""

from __future__ import annotations

import os
import sys
from PIL import Image

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
CHARTS_DIR = r"D:\InvisibleHandCharts"

DEFAULT_JOBS = [
    (
        os.path.join(CHARTS_DIR, "ArrowIndicatorSpritePink.jpg"),
        os.path.join(SCRIPT_DIR, "ArrowIndicatorSpritePink.png"),
        "colorized",
    ),
    (
        os.path.join(CHARTS_DIR, "ArrowIndicatorSpriteYellow.jpg"),
        os.path.join(SCRIPT_DIR, "ArrowIndicatorSpriteYellow.png"),
        "colorized",
    ),
    (
        os.path.join(CHARTS_DIR, "ArrowIndicatorSprite.jpg"),
        os.path.join(SCRIPT_DIR, "ArrowIndicatorSprite.png"),
        "filled",
    ),
    (
        os.path.join(CHARTS_DIR, "ArrowIndicatorSpriteSecondary.jpg"),
        os.path.join(SCRIPT_DIR, "ArrowIndicatorSpriteSecondary.png"),
        "line_overlay",
    ),
]

WHITE_KEY_THRESHOLD = 220
LINE_ALPHA_CUTOFF = 205


def is_background_pixel_colorized(r: int, g: int, b: int) -> bool:
    """Key near-white export + checkerboard neutrals; keep colored fill and black ink."""
    lum = int(0.299 * r + 0.587 * g + 0.114 * b)
    chroma = max(r, g, b) - min(r, g, b)
    if r > 240 and g > 240 and b > 240:
        return True
    if chroma < 14 and lum > 165:
        return True
    return False


def trim_to_alpha_bbox(img: Image.Image, padding: int = 1) -> Image.Image:
    px = img.load()
    w, h = img.size
    minx, maxx, miny, maxy = w, 0, h, 0
    found = False
    for y in range(h):
        for x in range(w):
            if px[x, y][3] > 12:
                found = True
                minx = min(minx, x)
                maxx = max(maxx, x)
                miny = min(miny, y)
                maxy = max(maxy, y)
    if not found:
        return img
    minx = max(0, minx - padding)
    miny = max(0, miny - padding)
    maxx = min(w - 1, maxx + padding)
    maxy = min(h - 1, maxy + padding)
    return img.crop((minx, miny, maxx + 1, maxy + 1))


def remove_alpha_specks(img: Image.Image, max_speck_area: int = 12) -> Image.Image:
    """Drop tiny disconnected opaque islands (JPEG fringe residue)."""
    px = img.load()
    w, h = img.size
    visited = [[False] * w for _ in range(h)]

    def neighbors(cx: int, cy: int):
        for ny in range(max(0, cy - 1), min(h, cy + 2)):
            for nx in range(max(0, cx - 1), min(w, cx + 2)):
                if nx == cx and ny == cy:
                    continue
                yield nx, ny

    for y in range(h):
        for x in range(w):
            if visited[y][x] or px[x, y][3] <= 12:
                continue
            stack = [(x, y)]
            component = []
            visited[y][x] = True
            while stack:
                cx, cy = stack.pop()
                component.append((cx, cy))
                for nx, ny in neighbors(cx, cy):
                    if visited[ny][nx] or px[nx, ny][3] <= 12:
                        continue
                    visited[ny][nx] = True
                    stack.append((nx, ny))
            if len(component) <= max_speck_area:
                for cx, cy in component:
                    px[cx, cy] = (255, 255, 255, 0)
    return img


def convert_colorized(src: str, dst: str) -> None:
    img = Image.open(src).convert("RGBA")
    px = img.load()
    w, h = img.size
    for y in range(h):
        for x in range(w):
            r, g, b, _a = px[x, y]
            if is_background_pixel_colorized(r, g, b):
                px[x, y] = (255, 255, 255, 0)
            else:
                px[x, y] = (r, g, b, 255)
    img = trim_to_alpha_bbox(img, padding=1)
    img = remove_alpha_specks(img, max_speck_area=12)
    w, h = img.size
    img.save(dst, "PNG")
    print(f"colorized    -> {dst} ({w}x{h})")


def convert_filled(src: str, dst: str) -> None:
    img = Image.open(src).convert("RGBA")
    px = img.load()
    w, h = img.size
    for y in range(h):
        for x in range(w):
            r, g, b, _a = px[x, y]
            lum = int(0.299 * r + 0.587 * g + 0.114 * b)
            if lum > WHITE_KEY_THRESHOLD:
                px[x, y] = (255, 255, 255, 0)
            else:
                px[x, y] = (lum, lum, lum, 255)
    img.save(dst, "PNG")
    print(f"filled       -> {dst} ({w}x{h})")


def convert_line_overlay(src: str, dst: str) -> None:
    img = Image.open(src).convert("RGBA")
    px = img.load()
    w, h = img.size
    for y in range(h):
        for x in range(w):
            r, g, b, _a = px[x, y]
            lum = int(0.299 * r + 0.587 * g + 0.114 * b)
            if lum > LINE_ALPHA_CUTOFF:
                px[x, y] = (255, 255, 255, 0)
            else:
                alpha = max(0, min(255, int((LINE_ALPHA_CUTOFF - lum) * 2.2)))
                px[x, y] = (255, 255, 255, alpha)
    img.save(dst, "PNG")
    print(f"line_overlay -> {dst} ({w}x{h})")


def run_job(src: str, dst: str, mode: str) -> None:
    if not os.path.isfile(src):
        raise FileNotFoundError(f"Source art not found: {src}")
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    if mode == "colorized":
        convert_colorized(src, dst)
    elif mode == "filled":
        convert_filled(src, dst)
    elif mode == "line_overlay":
        convert_line_overlay(src, dst)
    else:
        raise ValueError(f"Unknown mode '{mode}' (use colorized, filled, or line_overlay)")


def main() -> int:
    if len(sys.argv) == 1:
        for src, dst, mode in DEFAULT_JOBS:
            run_job(src, dst, mode)
        return 0

    if len(sys.argv) != 4:
        print(__doc__)
        return 2

    run_job(sys.argv[1], sys.argv[2], sys.argv[3])
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
