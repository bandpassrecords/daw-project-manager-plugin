"""The Standalone's icon: the app's icon with a plug-in badge (a puzzle piece,
the app's own symbol for its DAW plugin settings) in the bottom-right corner,
so it can't be mistaken for the app itself.

    python scripts/make_standalone_icon.py ../daw-project-manager/app_icon.png resources/brand/standalone_icon.png
"""
import re, sys
from PIL import Image, ImageDraw, ImageFilter

SRC, OUT = sys.argv[1], sys.argv[2]
SIZE = 1024
SS = 4  # supersampling for the vector shapes

# Material Icons "extension" (filled), 24x24 viewbox. Apache 2.0.
PUZZLE = ("M20.5 11H19V7c0-1.1-.9-2-2-2h-4V3.5C13 2.12 11.88 1 10.5 1S8 2.12 8 3.5V5H4c-1.1 0-1.99.9-1.99 2v3.8H3.5"
          "c1.49 0 2.7 1.21 2.7 2.7s-1.21 2.7-2.7 2.7H2V20c0 1.1.9 2 2 2h3.8v-1.5c0-1.49 1.21-2.7 2.7-2.7 1.49 0 2.7 1.21 2.7 2.7"
          "V22H17c1.1 0 2-.9 2-2v-4h1.5c1.38 0 2.5-1.12 2.5-2.5S21.88 11 20.5 11z")


def path_points(d, steps=24):
    tokens = re.findall(r'[MmHhVvCcSsLlZz]|-?(?:\d+\.?\d*|\.\d+)', d)
    pts, x, y, cmd, i = [], 0.0, 0.0, None, 0
    last_ctrl = None

    def num():
        nonlocal i
        v = float(tokens[i]); i += 1
        return v

    def cubic(p0, p1, p2, p3):
        for k in range(1, steps + 1):
            t = k / steps
            mt = 1 - t
            pts.append((mt**3 * p0[0] + 3 * mt * mt * t * p1[0] + 3 * mt * t * t * p2[0] + t**3 * p3[0],
                        mt**3 * p0[1] + 3 * mt * mt * t * p1[1] + 3 * mt * t * t * p2[1] + t**3 * p3[1]))

    while i < len(tokens):
        if re.match(r'[A-Za-z]', tokens[i]):
            cmd = tokens[i]; i += 1
        rel = cmd.islower()
        c = cmd.upper()
        if c == 'Z':
            last_ctrl = None
            continue
        if c == 'M':
            x, y = (x + num(), y + num()) if rel else (num(), num())
            pts.append((x, y)); cmd = 'l' if rel else 'L'; last_ctrl = None
        elif c == 'L':
            x, y = (x + num(), y + num()) if rel else (num(), num())
            pts.append((x, y)); last_ctrl = None
        elif c == 'H':
            x = x + num() if rel else num(); pts.append((x, y)); last_ctrl = None
        elif c == 'V':
            y = y + num() if rel else num(); pts.append((x, y)); last_ctrl = None
        elif c == 'C':
            a = [num() for _ in range(6)]
            if rel:
                a = [a[0] + x, a[1] + y, a[2] + x, a[3] + y, a[4] + x, a[5] + y]
            cubic((x, y), (a[0], a[1]), (a[2], a[3]), (a[4], a[5]))
            last_ctrl = (a[2], a[3]); x, y = a[4], a[5]
        elif c == 'S':
            a = [num() for _ in range(4)]
            if rel:
                a = [a[0] + x, a[1] + y, a[2] + x, a[3] + y]
            c1 = (2 * x - last_ctrl[0], 2 * y - last_ctrl[1]) if last_ctrl else (x, y)
            cubic((x, y), c1, (a[0], a[1]), (a[2], a[3]))
            last_ctrl = (a[0], a[1]); x, y = a[2], a[3]
    return pts


icon = Image.open(SRC).convert('RGBA').resize((SIZE, SIZE), Image.LANCZOS)

# The badge: a white disc in the corner, inside the icon's rounded edge.
big = SIZE * SS
cx = cy = 0.735 * big
r = 0.235 * big

shadow = Image.new('RGBA', (big, big), (0, 0, 0, 0))
ImageDraw.Draw(shadow).ellipse((cx - r, cy - r + 0.012 * big, cx + r, cy + r + 0.012 * big), fill=(0, 0, 0, 110))
shadow = shadow.filter(ImageFilter.GaussianBlur(0.012 * big))

badge = Image.new('RGBA', (big, big), (0, 0, 0, 0))
draw = ImageDraw.Draw(badge)
draw.ellipse((cx - r, cy - r, cx + r, cy + r), fill=(255, 255, 255, 255))

# The puzzle piece in the brand's orange, centred in the disc.
piece = 1.25 * r                       # the 24-unit viewbox spans this much
scale = piece / 24.0
ox, oy = cx - piece / 2, cy - piece / 2
poly = [(ox + px * scale, oy + py * scale) for px, py in path_points(PUZZLE)]
draw.polygon(poly, fill=(0xFF, 0x61, 0x00, 255))

layers = Image.alpha_composite(shadow, badge).resize((SIZE, SIZE), Image.LANCZOS)
out = Image.alpha_composite(icon, layers)
out.save(OUT, optimize=True)
print('saved', OUT, out.size)
