"""Generate the hl2box launcher icon: a Half-Life 2 style lambda on a dark
steel badge, with a GMod physgun-style energy arc and orb.

Drawn at 1024px with supersampling, then downscaled to every size we ship.
"""
from PIL import Image, ImageDraw, ImageFilter, ImageChops
import math, sys, os

S = 1024
OUT_DIR = sys.argv[1]
os.makedirs(OUT_DIR, exist_ok=True)

ORANGE_HI = (255, 196, 84)
ORANGE_LO = (214, 118, 22)
STEEL_TOP = (58, 64, 72)
STEEL_BOT = (16, 18, 22)
GMOD_BLUE = (92, 186, 255)
GMOD_CORE = (220, 242, 255)


def lerp(a, b, t):
    return tuple(int(round(a[i] + (b[i] - a[i]) * t)) for i in range(len(a)))


def vertical_gradient(size, top, bot):
    g = Image.new("RGBA", size)
    d = ImageDraw.Draw(g)
    for y in range(size[1]):
        d.line([(0, y), (size[0], y)], fill=lerp(top, bot, y / (size[1] - 1)) + (255,))
    return g


# --- base badge: rounded square, steel gradient -----------------------------
badge_mask = Image.new("L", (S, S), 0)
ImageDraw.Draw(badge_mask).rounded_rectangle([40, 40, S - 40, S - 40], radius=210, fill=255)
base = vertical_gradient((S, S), STEEL_TOP, STEEL_BOT)
canvas = Image.new("RGBA", (S, S), (0, 0, 0, 0))
canvas.paste(base, (0, 0), badge_mask)

# bevel: light top edge, dark bottom edge (Valve panel look)
bevel = Image.new("RGBA", (S, S), (0, 0, 0, 0))
bd = ImageDraw.Draw(bevel)
bd.rounded_rectangle([40, 40, S - 40, S - 40], radius=210, outline=(120, 130, 142, 150), width=10)
bd.rounded_rectangle([54, 54, S - 54, S - 54], radius=196, outline=(0, 0, 0, 120), width=6)
canvas = Image.alpha_composite(canvas, bevel)

# faint horizontal scanline texture, very subtle
tex = Image.new("RGBA", (S, S), (0, 0, 0, 0))
td = ImageDraw.Draw(tex)
for y in range(96, S - 96, 8):
    td.line([(96, y), (S - 96, y)], fill=(255, 255, 255, 7))
canvas.alpha_composite(tex)
canvas.putalpha(Image.composite(canvas.getchannel("A"), Image.new("L", (S, S), 0), badge_mask))

# --- GMod physgun energy: a tilted ellipse arc + glowing orb ------------------
def ring_points(cx, cy, rx, ry, rot_deg, start, end, n=240):
    pts = []
    rot = math.radians(rot_deg)
    for i in range(n + 1):
        a = math.radians(start + (end - start) * i / n)
        x = rx * math.cos(a)
        y = ry * math.sin(a)
        pts.append((cx + x * math.cos(rot) - y * math.sin(rot),
                    cy + x * math.sin(rot) + y * math.cos(rot)))
    return pts

glow = Image.new("RGBA", (S, S), (0, 0, 0, 0))
gd = ImageDraw.Draw(glow)
arc = ring_points(512, 512, 430, 165, -24, 200, 390)
gd.line(arc, fill=GMOD_BLUE + (255,), width=34, joint="curve")
arc2 = ring_points(512, 512, 430, 165, -24, 200, 300)
gd.line(arc2, fill=GMOD_CORE + (255,), width=10, joint="curve")
blur_glow = glow.filter(ImageFilter.GaussianBlur(26))
canvas = Image.alpha_composite(canvas, blur_glow)
canvas = Image.alpha_composite(canvas, glow.filter(ImageFilter.GaussianBlur(3)))

# orb at the front of the arc
orb_layer = Image.new("RGBA", (S, S), (0, 0, 0, 0))
ox, oy = ring_points(512, 512, 430, 165, -24, 300, 300, 1)[0]
for r, a in [(92, 70), (62, 130), (36, 230), (18, 255)]:
    col = lerp(GMOD_BLUE, GMOD_CORE, 1 - r / 92) + (a,)
    ImageDraw.Draw(orb_layer).ellipse([ox - r, oy - r, ox + r, oy + r], fill=col)
canvas = Image.alpha_composite(canvas, orb_layer.filter(ImageFilter.GaussianBlur(6)))

# --- the lambda ---------------------------------------------------------------
def stroke_poly(p0, p1, w):
    (x0, y0), (x1, y1) = p0, p1
    dx, dy = x1 - x0, y1 - y0
    L = math.hypot(dx, dy)
    nx, ny = -dy / L * w / 2, dx / L * w / 2
    return [(x0 + nx, y0 + ny), (x1 + nx, y1 + ny), (x1 - nx, y1 - ny), (x0 - nx, y0 - ny)]

LONG = ((318, 226), (742, 796))
t = 0.55
meet = (LONG[0][0] + (LONG[1][0] - LONG[0][0]) * t, LONG[0][1] + (LONG[1][1] - LONG[0][1]) * t)
SHORT = ((326, 800), meet)
W = 104

lam_mask = Image.new("L", (S, S), 0)
lmd = ImageDraw.Draw(lam_mask)
lmd.polygon(stroke_poly(*LONG, W), fill=255)
lmd.polygon(stroke_poly(*SHORT, W), fill=255)
# round the joints a little so the shape stays solid where the strokes meet
lmd.ellipse([meet[0] - W / 2, meet[1] - W / 2, meet[0] + W / 2, meet[1] + W / 2], fill=255)

# drop shadow for depth
shadow = lam_mask.filter(ImageFilter.GaussianBlur(14)).point(lambda v: int(v * 0.8))
shadow_layer = Image.new("RGBA", (S, S), (0, 0, 0, 0))
shadow_layer.paste((0, 0, 0, 255), (0, 18), shadow)
canvas = Image.alpha_composite(canvas, shadow_layer)

# orange gradient fill, clipped to the lambda
grad = vertical_gradient((S, S), ORANGE_HI, ORANGE_LO)
lam_layer = Image.new("RGBA", (S, S), (0, 0, 0, 0))
lam_layer.paste(grad, (0, 0), lam_mask)
canvas = Image.alpha_composite(canvas, lam_layer)

# top highlight stripe inside the lambda (HL2 logo sheen)
hi_mask = Image.new("L", (S, S), 0)
hmd = ImageDraw.Draw(hi_mask)
hmd.polygon(stroke_poly(LONG[0], (LONG[0][0] + (LONG[1][0] - LONG[0][0]) * 0.5,
                                  LONG[0][1] + (LONG[1][1] - LONG[0][1]) * 0.5), W * 0.22), fill=120)
hi_mask = ImageChops.multiply(hi_mask, lam_mask)
hi_layer = Image.new("RGBA", (S, S), (255, 245, 215, 0))
hi_layer.putalpha(hi_mask)
canvas = Image.alpha_composite(canvas, hi_layer)

# --- write the sizes ------------------------------------------------------------
final = canvas.convert("RGBA")
final.resize((512, 512), Image.LANCZOS).save(os.path.join(OUT_DIR, "logo-512.png"))
final.resize((256, 256), Image.LANCZOS).save(os.path.join(OUT_DIR, "logo-256.png"))
final.resize((128, 128), Image.LANCZOS).save(os.path.join(OUT_DIR, "logo-128.png"))
ico_sizes = [16, 24, 32, 48, 64, 128, 256]
final.save(os.path.join(OUT_DIR, "launcher.ico"), sizes=[(s, s) for s in ico_sizes])
print("icon written to", OUT_DIR)
