#!/usr/bin/env python3
"""
Generate OLED display mockup JPGs for all ClockBox v3 display screens.
Native display: 128x64 pixels, OLED-style (black bg, cyan-blue text)
Rendered at 4x scale for clarity.
"""

from PIL import Image, ImageDraw, ImageFont
import os

OUT_DIR = os.path.join(os.path.dirname(__file__), "images", "display")
os.makedirs(OUT_DIR, exist_ok=True)

SCALE = 4          # upscale factor
W, H  = 128, 64   # native display size
SW, SH = W * SCALE, H * SCALE   # 512 x 256

BG  = (0, 0, 0)
FG  = (130, 210, 255)   # OLED cyan-blue


# Font sizes matching the firmware's font usage:
#   Verdana_digits_24 at 2X  → ~48px native → big BPM digits  (use Verdana Bold)
#   ZevvPeep8x16 at 2X       → ~16px native → big header       (use Verdana Bold)
#   ZevvPeep8x16 at 1X       → ~8px native  → small labels     (use Andale Mono)


def load_font(path, native_size_px):
    size = native_size_px * SCALE
    try:
        return ImageFont.truetype(path, size)
    except Exception:
        return ImageFont.load_default()


VERDANA_BOLD = "/System/Library/Fonts/Supplemental/Verdana Bold.ttf"
ANDALE_MONO  = "/System/Library/Fonts/Supplemental/Andale Mono.ttf"

f_bpm    = load_font(VERDANA_BOLD, 46)   # large BPM / large value
f_header = load_font(VERDANA_BOLD, 20)   # 2X header "ClockBox"
f_small  = load_font(ANDALE_MONO,   8)   # small 1X pixel-style labels


def n(px):
    """Scale native pixel to canvas pixel."""
    return px * SCALE


def make_canvas():
    img = Image.new("RGB", (SW, SH), BG)
    draw = ImageDraw.Draw(img)
    return img, draw


def blinker(draw):
    """Beat blinker: inverted block at col 112, row 0 (~16x10 native px)."""
    draw.rectangle([n(112), n(0), n(128) - 1, n(10)], fill=FG)


def save(img, name):
    bezel = 8 * SCALE
    total_w = SW + bezel * 2
    total_h = SH + bezel * 2
    out = Image.new("RGB", (total_w, total_h), (18, 18, 18))
    draw_b = ImageDraw.Draw(out)
    # subtle inner glow on bezel
    for i in range(bezel):
        v = int(18 + (i / bezel) * 8)
        draw_b.rectangle([i, i, total_w - 1 - i, total_h - 1 - i],
                         outline=(v, v, v))
    out.paste(img, (bezel, bezel))
    path = os.path.join(OUT_DIR, name + ".jpg")
    out.save(path, "JPEG", quality=94)
    print(f"  saved: {path}")


# ── Helper: draw text at native (x, y) coords ──────────────────────────────
def put(draw, x_native, y_native, text, font, color=FG):
    draw.text((n(x_native), n(y_native)), text, font=font, fill=color)


# ── Screens ─────────────────────────────────────────────────────────────────

def screen_boot():
    """Power-on info: ClockBox / Version / socialmidi.com"""
    img, draw = make_canvas()
    put(draw,  4,  2, "ClockBox",       f_header)
    put(draw,  4, 30, "  Version 3.54", f_small)
    put(draw,  4, 44, " socialmidi.com", f_small)
    save(img, "screen_boot")


def screen_qrs_start(bpm=120):
    """Standalone A – QRS Start"""
    img, draw = make_canvas()
    x = 30 if bpm < 100 else 15
    put(draw, x,  0, str(bpm),   f_bpm)
    put(draw, 1, 48, "QRS Start", f_small)
    blinker(draw)
    save(img, "screen_qrs_start")


def screen_qrs_stopstart(bpm=120):
    """Standalone B – QRS Stop Start"""
    img, draw = make_canvas()
    x = 30 if bpm < 100 else 15
    put(draw, x,  0, str(bpm),          f_bpm)
    put(draw, 1, 48, "QRS Stop Start",  f_small)
    blinker(draw)
    save(img, "screen_qrs_stopstart")


def screen_follow_din():
    """Follow Clock from DIN MIDI"""
    img, draw = make_canvas()
    put(draw, 1,  8, "Follow Clock",   f_small)
    put(draw, 1, 24, "from DIN Midi",  f_small)
    put(draw, 1, 48, "ext.Clk DIN 24", f_small)
    blinker(draw)
    save(img, "screen_follow_din")


def screen_follow_usb():
    """Follow Clock from USB MIDI"""
    img, draw = make_canvas()
    put(draw, 1,  8, "Follow Clock",    f_small)
    put(draw, 1, 24, "from USB Midi",   f_small)
    put(draw, 1, 48, "ext.Clk USB 24",  f_small)
    blinker(draw)
    save(img, "screen_follow_usb")


def screen_qrs_offset(val=2):
    """QRS Offset editor"""
    img, draw = make_canvas()
    put(draw, 0,  0, "QRS Offset(PPQN):", f_small)
    x = 30 if val < 100 else 15
    put(draw, x, 14, str(val), f_bpm)
    save(img, "screen_qrs_offset")


def screen_clock_divider(val=1):
    """Clock Divider editor"""
    img, draw = make_canvas()
    put(draw,  0,  0, "Clock Divider:", f_small)
    put(draw, 40, 14, f"/{val}",        f_bpm)
    save(img, "screen_clock_divider")


def screen_led_brightness(val=50):
    """LED Brightness editor"""
    img, draw = make_canvas()
    put(draw,  0,  0, "LED Brightness:", f_small)
    x = 30 if val < 100 else 15
    put(draw, x, 14, str(val), f_bpm)
    save(img, "screen_led_brightness")


def screen_update_mode():
    """Firmware update mode"""
    img, draw = make_canvas()
    put(draw,  4,  2, "ClockBox",    f_header)
    put(draw,  4, 42, "  UpdateMode", f_small)
    save(img, "screen_update_mode")


def screen_mode_reset():
    """Clock mode factory reset"""
    img, draw = make_canvas()
    put(draw,  4,  2, "ClockBox",       f_header)
    put(draw,  4, 42, "ClockMode reset", f_small)
    save(img, "screen_mode_reset")


if __name__ == "__main__":
    print("Generating display screens...")
    screen_boot()
    screen_qrs_start(120)
    screen_qrs_stopstart(120)
    screen_follow_din()
    screen_follow_usb()
    screen_qrs_offset(2)
    screen_clock_divider(1)
    screen_led_brightness(50)
    screen_update_mode()
    screen_mode_reset()
    print("Done. Output: images/display/")
