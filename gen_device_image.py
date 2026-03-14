#!/usr/bin/env python3
"""
Render a clean top-view illustration of the ClockBox v3.
Yellow 3D-printed enclosure, top panel.
"""

from PIL import Image, ImageDraw, ImageFont
import math, os

OUT = os.path.join(os.path.dirname(__file__), "images", "device_top_view.jpg")

# ── Canvas ───────────────────────────────────────────────────────────────────
W, H = 900, 860
img  = Image.new("RGB", (W, H), (30, 30, 30))
d    = ImageDraw.Draw(img)

# ── Colours ──────────────────────────────────────────────────────────────────
YELLOW      = (248, 196,  30)
YELLOW_DARK = (210, 162,  10)
YELLOW_MID  = (232, 180,  20)
YELLOW_LITE = (255, 220,  80)
BLACK       = (  0,   0,   0)
NEAR_BLACK  = ( 28,  28,  28)
DISPLAY_BG  = (  5,   8,  12)
OLED_FG     = (130, 210, 255)
BUTTON_TOP  = (252, 204,  40)
BUTTON_EDGE = (180, 138,   8)
SHADOW      = ( 20,  20,  20)
LED_OFF     = ( 60,  55,  20)
LED_GREEN   = ( 60, 220,  80)
LED_BLUE    = ( 60, 130, 255)
LED_CYAN    = (100, 220, 255)

# ── Fonts ─────────────────────────────────────────────────────────────────────
def font(name, size):
    try:
        return ImageFont.truetype(name, size)
    except Exception:
        return ImageFont.load_default()

VERDANA      = "/System/Library/Fonts/Supplemental/Verdana.ttf"
VERDANA_BOLD = "/System/Library/Fonts/Supplemental/Verdana Bold.ttf"
ANDALE       = "/System/Library/Fonts/Supplemental/Andale Mono.ttf"

f_label   = font(VERDANA_BOLD, 22)
f_label_s = font(VERDANA,      17)
f_bpm     = font(VERDANA_BOLD, 54)
f_bpm_sm  = font(ANDALE,       13)
f_brand   = font(VERDANA_BOLD, 19)

# ── Helpers ──────────────────────────────────────────────────────────────────
def ellipse(cx, cy, rx, ry, fill, outline=None, width=1):
    d.ellipse([cx-rx, cy-ry, cx+rx, cy+ry], fill=fill,
              outline=outline, width=width)

def roundrect(x0, y0, x1, y1, r, fill, outline=None, width=1):
    d.rounded_rectangle([x0, y0, x1, y1], radius=r,
                        fill=fill, outline=outline, width=width)

def centered_text(cx, cy, text, f, color=BLACK):
    bb = d.textbbox((0, 0), text, font=f)
    tw, th = bb[2]-bb[0], bb[3]-bb[1]
    d.text((cx - tw//2, cy - th//2), text, font=f, fill=color)

# ── Drop shadow ───────────────────────────────────────────────────────────────
for i in range(14, 0, -1):
    alpha = int(180 * (1 - i/14))
    d.rounded_rectangle([60+i, 60+i, W-60+i, H-60+i],
                        radius=48, outline=(SHADOW[0], SHADOW[1], SHADOW[2]),
                        width=1)

# ── Box body ──────────────────────────────────────────────────────────────────
BOX_X0, BOX_Y0 = 58, 56
BOX_X1, BOX_Y1 = W-58, H-56
R = 52   # corner radius

# side shading (3-D feel)
roundrect(BOX_X0+6, BOX_Y0+6, BOX_X1+6, BOX_Y1+6, R, YELLOW_DARK)
roundrect(BOX_X0,   BOX_Y0,   BOX_X1,   BOX_Y1,   R, YELLOW)

# subtle top-surface gradient bands
for i in range(40):
    brightness = int(248 - i * 0.5)
    d.line([(BOX_X0+R, BOX_Y0+i), (BOX_X1-R, BOX_Y0+i)],
           fill=(brightness, int(196 - i*0.3), 30), width=1)

# re-draw crisp outline
roundrect(BOX_X0, BOX_Y0, BOX_X1, BOX_Y1, R,
          fill=None, outline=YELLOW_DARK, width=3)

# ── Layout constants ──────────────────────────────────────────────────────────
CX = W // 2   # horizontal centre of box

# ── 5 NeoPixel LEDs (top row) ─────────────────────────────────────────────────
LED_Y  = 108
LED_R  = 10
LED_COLORS = [LED_GREEN, LED_BLUE, LED_BLUE, LED_BLUE, LED_CYAN]
LED_XS = [CX - 110, CX - 55, CX, CX + 55, CX + 110]

for lx, lc in zip(LED_XS, LED_COLORS):
    # glow halo
    for halo in range(8, 0, -1):
        alpha = int(60 * (1 - halo/8))
        ellipse(lx, LED_Y, LED_R+halo, LED_R+halo,
                fill=(lc[0], lc[1], lc[2]))
    ellipse(lx, LED_Y, LED_R, LED_R, fill=lc, outline=NEAR_BLACK, width=1)
    # specular spot
    ellipse(lx-3, LED_Y-3, 3, 3, fill=(255, 255, 255))

# ── TAP button (top-left) ─────────────────────────────────────────────────────
TAP_X, TAP_Y, TAP_R = 168, 270, 40
ellipse(TAP_X+3, TAP_Y+4, TAP_R, TAP_R, fill=BUTTON_EDGE)          # shadow
ellipse(TAP_X,   TAP_Y,   TAP_R, TAP_R, fill=BUTTON_TOP, outline=YELLOW_DARK, width=2)
ellipse(TAP_X,   TAP_Y,   TAP_R-8, TAP_R-8, fill=YELLOW_LITE)       # raised dome
centered_text(TAP_X, TAP_Y+62, "tap", f_label)

# ── OLED display ──────────────────────────────────────────────────────────────
DSP_X0, DSP_Y0 = CX-88, 180
DSP_X1, DSP_Y1 = CX+88, 300

# bezel
roundrect(DSP_X0-6, DSP_Y0-6, DSP_X1+6, DSP_Y1+6, 6,
          fill=NEAR_BLACK, outline=(50,50,50), width=2)
# screen
d.rectangle([DSP_X0, DSP_Y0, DSP_X1, DSP_Y1], fill=DISPLAY_BG)
# OLED content: BPM
bb = d.textbbox((0,0), "120", font=f_bpm)
bw = bb[2]-bb[0]
d.text((DSP_X0 + (DSP_X1-DSP_X0-bw)//2, DSP_Y0 + 4),
       "120", font=f_bpm, fill=OLED_FG)
# bottom label
d.text((DSP_X0 + 4, DSP_Y1 - 20),
       "QRS Stop Start", font=f_bpm_sm, fill=OLED_FG)
# blinker top-right
d.rectangle([DSP_X1-18, DSP_Y0+2, DSP_X1-2, DSP_Y0+12], fill=OLED_FG)

# ── Encoder (top-right) ───────────────────────────────────────────────────────
ENC_X, ENC_Y, ENC_R = W-168, 248, 56
ellipse(ENC_X+4, ENC_Y+5, ENC_R, ENC_R, fill=YELLOW_DARK)            # shadow
ellipse(ENC_X,   ENC_Y,   ENC_R, ENC_R, fill=(52,52,52), outline=(30,30,30), width=2)
# knurling lines
for angle_deg in range(0, 360, 20):
    a = math.radians(angle_deg)
    ix = ENC_X + int((ENC_R-6) * math.cos(a))
    iy = ENC_Y + int((ENC_R-6) * math.sin(a))
    ox = ENC_X + int(ENC_R * math.cos(a))
    oy = ENC_Y + int(ENC_R * math.sin(a))
    d.line([(ix, iy), (ox, oy)], fill=(70,70,70), width=2)
# centre cap
ellipse(ENC_X, ENC_Y, ENC_R-18, ENC_R-18, fill=(45,45,45))
# indicator mark (12 o'clock)
d.line([(ENC_X, ENC_Y - (ENC_R-20)), (ENC_X, ENC_Y - (ENC_R-5))],
       fill=(220, 220, 220), width=3)
centered_text(ENC_X, ENC_Y - ENC_R - 18, "bpm",    f_label_s)
centered_text(ENC_X, ENC_Y + ENC_R + 16, "select", f_label_s)

# ── Bottom row buttons: PLAY, PRESET1, PRESET2, PRESET3, STOP ────────────────
BTN_Y  = 590
BTN_R  = 38

btn_specs = [
    (168,          "play",    BUTTON_TOP),
    (CX - 100,     "1",       BUTTON_TOP),
    (CX,           "2",       BUTTON_TOP),
    (CX + 100,     "3",       BUTTON_TOP),
    (W - 168,      "stop",    BUTTON_TOP),
]

# "presets" label above preset buttons
centered_text(CX, BTN_Y - BTN_R - 24, "presets", f_label)

for bx, label, color in btn_specs:
    ellipse(bx+3, BTN_Y+4, BTN_R, BTN_R, fill=BUTTON_EDGE)
    ellipse(bx,   BTN_Y,   BTN_R, BTN_R, fill=color, outline=YELLOW_DARK, width=2)
    ellipse(bx,   BTN_Y,   BTN_R-8, BTN_R-8, fill=YELLOW_LITE)
    centered_text(bx, BTN_Y + BTN_R + 20,
                  label, f_label if len(label) > 1 else f_label)

# ── "play" and "stop" labels already in btn_specs loop above
# ── Brand text ────────────────────────────────────────────────────────────────
centered_text(CX, BTN_Y + BTN_R + 56, "socialmidi.com", f_brand)

# ── Save ──────────────────────────────────────────────────────────────────────
img.save(OUT, "JPEG", quality=94)
print(f"saved: {OUT}")
