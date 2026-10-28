#!/usr/bin/env python3
import sys
from pathlib import Path
from PIL import Image

NAMES = ["terminal", "files", "settings"]

RECOLOR = {"terminal": (255, 255, 255), "settings": (255, 255, 255)}

def main():
    in_dir, out_dir = Path(sys.argv[1]), Path(sys.argv[2])
    size = int(sys.argv[3]) if len(sys.argv) > 3 else 64
    out_dir.mkdir(parents=True, exist_ok=True)

    for name in NAMES:
        src = next((in_dir / f"{name}{e}" for e in (".png", ".webp", ".jpg", ".jpeg")
                    if (in_dir / f"{name}{e}").exists()), None)
        if not src:
            print(f"WARNING: manca {name}"); continue

        im = Image.open(src).convert("RGBA")


        if im.getchannel("A").getextrema() == (255, 255):
            bg = im.getpixel((0, 0))[:3]
            diff = Image.new("RGB", im.size, bg)
            from PIL import ImageChops
            mask = ImageChops.difference(im.convert("RGB"), diff).convert("L").point(lambda v: 255 if v > 30 else 0)
            im.putalpha(mask)


        bbox = im.getchannel("A").point(lambda v: 255 if v > 8 else 0).getbbox()
        if bbox: im = im.crop(bbox)
        side = int(max(im.size) * 1.16)
        canvas = Image.new("RGBA", (side, side), (0, 0, 0, 0))
        canvas.paste(im, ((side - im.width) // 2, (side - im.height) // 2))


        if name in RECOLOR:
            r, g, b = RECOLOR[name]
            a = canvas.getchannel("A")
            canvas = Image.new("RGBA", canvas.size, (r, g, b, 0))
            canvas.putalpha(a)


        out = canvas.resize((size, size), Image.LANCZOS)
        dst = out_dir / f"{name.upper()}.PNG"
        out.save(dst, "PNG")
        print(f"{src.name} -> {dst} ({size}x{size})")

main()