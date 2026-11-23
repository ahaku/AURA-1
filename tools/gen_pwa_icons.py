"""Generate the shared AURA-1 favicon and installable app icons."""
import hashlib
from pathlib import Path
import re
import shutil

from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
BACKGROUND = "#242820"
FOREGROUND = "#c6d19c"
STROKES = (((18, 45), (29, 19), (35, 19), (46, 45)), ((23, 34), (41, 34)))
SVG = (
    '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64">\n'
    f'  <rect width="64" height="64" rx="14" fill="{BACKGROUND}"/>\n'
    f'  <g fill="none" stroke="{FOREGROUND}" stroke-width="6" stroke-linecap="round" stroke-linejoin="round">\n'
    + "".join('    <polyline points="' + " ".join(f"{x},{y}" for x, y in points) + '"/>\n' for points in STROKES)
    + '  </g>\n</svg>\n'
)


def create_icon(size, rounded=False):
    scale = max(size, 256) / 64
    canvas = int(64 * scale)
    image = Image.new("RGBA", (canvas, canvas), BACKGROUND if not rounded else (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    if rounded:
        draw.rounded_rectangle((0, 0, canvas - 1, canvas - 1), radius=14 * scale, fill=BACKGROUND)
    for stroke in STROKES:
        points = [(x * scale, y * scale) for x, y in stroke]
        draw.line(points, fill=FOREGROUND, width=int(6 * scale), joint="curve")
        for x, y in points:
            r = 3 * scale
            draw.ellipse((x - r, y - r, x + r, y + r), fill=FOREGROUND)
    return image.resize((size, size), Image.Resampling.LANCZOS)


def main():
    site = ROOT / "web/site"
    (site / "icon.svg").write_text(SVG, encoding="utf-8", newline="\n")
    create_icon(48, rounded=True).save(site / "favicon.ico", sizes=[(16, 16), (32, 32), (48, 48)])
    create_icon(180).save(site / "apple-touch-icon.png", optimize=True)
    version = hashlib.sha256(SVG.encode()).hexdigest()[:12]
    for destination in (ROOT / "app/web/emu", ROOT / "web/emu"):
        (destination / "icon.svg").write_text(SVG, encoding="utf-8", newline="\n")
        for size in (192, 512):
            create_icon(size).save(destination / f"icon-{size}.png", optimize=True)
        shutil.copy2(site / "favicon.ico", destination / "favicon.ico")
    page = ROOT / "app/web/emu/index.html"
    content = page.read_text(encoding="utf-8")
    content = re.sub(r'href="icon\.svg(?:\?v=[a-f0-9]+)?"', f'href="icon.svg?v={version}"', content)
    page.write_text(content, encoding="utf-8", newline="\n")
    print("Generated AURA-1 SVG, ICO, Apple touch and PWA icons")


if __name__ == "__main__":
    main()
