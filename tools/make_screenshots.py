# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Patryk Strzelecki <patryk@strzelecki.info>
#
# Renders mock-ups of the firmware screens for the README (docs/images/*.png).
# Layout, sizes, colours and texts follow src/ui (Theme.h, Strings.h, the screen classes);
# fonts are approximated with Arial. Usage: python tools/make_screenshots.py
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "docs" / "images"
FONT_DIR = Path("C:/Windows/Fonts")
S = 2  # pixel scale of the 240x135 screen

BG = (10, 10, 14)
SURFACE = (28, 28, 36)
SURFACE_HIGH = (48, 48, 60)
TEXT = (236, 236, 240)
MUTED = (140, 140, 155)
DISABLED = (80, 80, 92)
BRAND_ACCENT = (226, 36, 74)
OK = (60, 200, 110)
WARN = (245, 170, 40)
ERROR = (235, 70, 60)

RECORD = (214, 40, 56)
RECORD_LISTENING = (110, 24, 34)
LIBRARY = (30, 120, 230)
MIC_CHECK = (20, 150, 140)
PAUSED = (200, 130, 20)

TINT_RECORD = (40, 8, 12)
TINT_LIBRARY = (8, 22, 46)
TINT_MIC_CHECK = (5, 30, 28)


def font(px, bold=False):
    return ImageFont.truetype(str(FONT_DIR / ("arialbd.ttf" if bold else "arial.ttf")), int(px * S))


F0 = lambda: font(7.5)          # Font0 (6x8)
F0B = lambda: font(7.5, True)
F2 = lambda: font(10.5)         # Font2 (16 px)
F9 = lambda: font(11)           # FreeSans9pt
F9B = lambda: font(11, True)    # FreeSansBold9pt
F12B = lambda: font(15, True)   # FreeSansBold12pt
F18B = lambda: font(21, True)   # FreeSansBold18pt


class Screen:
    def __init__(self, bg=BG):
        self.image = Image.new("RGB", (240 * S, 135 * S), bg)
        self.d = ImageDraw.Draw(self.image)

    def r(self, x0, y0, x1, y1, fill, radius=0, outline=None):
        box = [x0 * S, y0 * S, x1 * S, y1 * S]
        if radius:
            self.d.rounded_rectangle(box, radius=radius * S, fill=fill, outline=outline, width=S)
        else:
            self.d.rectangle(box, fill=fill, outline=outline)

    def t(self, x, y, text, f, fill=TEXT, anchor="la"):
        self.d.text((x * S, y * S), text, font=f, fill=fill, anchor=anchor)

    def circle(self, cx, cy, radius, fill):
        self.d.ellipse([(cx - radius) * S, (cy - radius) * S, (cx + radius) * S, (cy + radius) * S], fill=fill)

    def line(self, points, fill, width=1.0):
        self.d.line([(x * S, y * S) for x, y in points], fill=fill, width=max(1, int(width * S)))

    # --- common parts -------------------------------------------------------------
    def status_bar(self, chip, chip_colour, text, dot=OK):
        self.r(0, 0, 240, 18, SURFACE)
        f = F0B()
        width = self.d.textlength(chip, font=f) / S + 12
        self.r(2, 2, 2 + width, 16, chip_colour, radius=4)
        self.t(2 + width / 2, 9, chip, f, anchor="mm")
        self.circle(2 + width + 8, 9, 3, dot)
        self.t(2 + width + 15, 9, text, F0(), anchor="lm")
        # Battery on right
        self.r(214, 5, 232, 14, None, outline=TEXT)
        self.r(232, 7, 234, 12, TEXT)
        self.r(216, 7, 228, 12, TEXT)
        self.t(211, 9, "87%", F0(), MUTED, anchor="rm")

    def footer(self, text):
        self.r(0, 119, 240, 135, SURFACE)
        self.t(120, 127, text, F0(), MUTED, anchor="mm")

    def band(self, colour, title, icon_func, right_text=None, right_f=None, dot_x=None):
        self.r(0, 18, 240, 40, colour)
        icon_func(16, 29, 0.42, TEXT, colour)
        self.t(32, 30, title, F9B(), anchor="lm")
        if right_text:
            f = right_f if right_f else F0()
            self.t(234, 30, right_text, f, anchor="rm")
        if dot_x is not None:
            self.circle(dot_x, 29, 4, TEXT)

    def key_hints(self, top, hints):
        f, lf = F0B(), F0()
        widths = []
        for key, label in hints:
            cap = max(16, self.d.textlength(key, font=f) / S + 8)
            widths.append((cap, cap + 4 + self.d.textlength(label, font=lf) / S))
        gap = (240 - sum(w for _, w in widths)) / (len(hints) + 1)
        x = gap
        cy = top + 9
        for (key, label), (cap, total) in zip(hints, widths):
            self.r(x, cy - 6.5, x + cap, cy + 7.5, SURFACE, radius=3)
            self.r(x, cy - 7.5, x + cap, cy + 5.5, TEXT, radius=3)
            self.t(x + cap / 2, cy - 1, key, f, BG, anchor="mm")
            self.t(x + cap + 4, cy, label, lf, anchor="lm")
            x += total + gap

    def meter_x(self, x, width, db):
        clamped = max(-70.0, min(0.0, db))
        return x + ((clamped - (-70.0)) / 70.0) * (width - 1)

    def level_meter(self, x, y, width, height, level_db, peak_db, colour, noise_db=None, trigger_db=None):
        self.r(x, y, x + width, y + height, SURFACE, radius=3)
        lx = self.meter_x(x, width, level_db)
        if level_db > -70.0 and lx > x:
            self.r(x, y, lx + 1, y + height, colour, radius=3)
        # scale ticks every 10 dB
        for db in range(-60, 0, 10):
            tx = self.meter_x(x, width, db)
            self.line([(tx, y + height - 3), (tx, y + height)], BG, width=1)
        if noise_db is not None and noise_db <= 0.0:
            nx = self.meter_x(x, width, noise_db)
            self.r(nx - 1, y, nx + 1, y + height, MUTED)
        if peak_db is not None and peak_db > -70.0:
            px = self.meter_x(x, width, peak_db)
            self.line([(px, y), (px, y + height)], TEXT, width=1)
        if trigger_db is not None and trigger_db <= 0.0:
            tx = self.meter_x(x, width, trigger_db)
            self.d.polygon([(tx - 4) * S, (y - 5) * S, (tx + 4) * S, (y - 5) * S, tx * S, (y - 1) * S], fill=TEXT)
            self.line([(tx, y), (tx, y + height)], TEXT, width=1)

    def progress_bar(self, x, y, width, progress, colour):
        progress = max(0.0, min(1.0, progress))
        self.r(x, y - 2, x + width, y + 2, SURFACE_HIGH, radius=2)
        filled = int(progress * width)
        if filled > 0:
            self.r(x, y - 2, x + filled, y + 2, colour, radius=2)
        self.circle(x + filled, y, 5, TEXT)

    # --- icons -------------------------------------------------------------------
    def mic_icon(self, cx, cy, k, colour, bg):
        cw, ch = 14 * k, 22 * k
        self.r(cx - cw / 2, cy - 17 * k, cx + cw / 2, cy - 17 * k + ch, colour, radius=cw / 2)
        r = 12 * k
        w = max(2, int(3 * k * S))
        self.d.arc([(cx - r) * S, (cy - 2 * k - r) * S, (cx + r) * S, (cy - 2 * k + r) * S], 0, 180, fill=colour, width=w)
        sw = max(2, 3 * k)
        self.r(cx - sw / 2, cy + 10 * k, cx + sw / 2, cy + 16 * k, colour)
        self.r(cx - 8 * k, cy + 15 * k, cx + 8 * k, cy + 15 * k + sw, colour, radius=1)
        for i in range(3):
            gy = cy - (12 - i * 5) * k
            self.r(cx - 3 * k, gy, cx + 3 * k, gy + max(1, 1.5 * k), bg)

    def levels_icon(self, cx, cy, k, colour, bg=None):
        heights = [10, 18, 30, 22, 14]
        bw = max(2, 5 * k)
        gap = max(1, 2.5 * k)
        total = 5 * bw + 4 * gap
        x = cx - total / 2
        bottom = cy + 15 * k
        for h in heights:
            bh = h * k
            self.r(x, bottom - bh, x + bw, bottom, colour, radius=max(1, bw / 3))
            x += bw + gap

    def play_icon(self, cx, cy, k, colour, bg=None):
        pts = [(cx - 10 * k, cy - 14 * k), (cx - 10 * k, cy + 14 * k), (cx + 14 * k, cy)]
        self.d.polygon([(x * S, y * S) for x, y in pts], fill=colour)

    def save(self, name):
        OUT.mkdir(parents=True, exist_ok=True)
        self.image.save(OUT / name, optimize=True)


# --- screen generators -------------------------------------------------------
def record_screen():
    s = Screen(TINT_RECORD)
    s.status_bar("REC", RECORD, "8 recordings, 14.8 GB free", OK)
    # Band: "RECORDING", elapsed time "00:14", blinking dot before time
    time_w = s.d.textlength("00:14", font=F2()) / S
    dot_x = 234 - time_w - 8
    s.band(RECORD, "RECORDING", s.mic_icon, right_text="00:14", right_f=F2(), dot_x=dot_x)

    # Middle area
    s.t(8, 63, "00:14", F18B(), anchor="lm")
    # File details on right
    s.t(234, 45, "rec_0009", F0(), TEXT, anchor="ra")
    s.t(234, 57, "112 KB, auto +12", F0(), MUTED, anchor="ra")
    s.t(234, 69, "silence skipped 00:08", F0(), MUTED, anchor="ra")

    # Meter row (y = 91, height = 8, width = 200)
    s.level_meter(6, 91, 200, 8, level_db=-22.0, peak_db=-18.0, colour=OK, noise_db=-52.0, trigger_db=-40.0)
    s.t(234, 95, "-22", F0(), MUTED, anchor="rm")

    # Key row (top = 101, height = 18)
    s.key_hints(101, [("ENTER", "Save"), ("SPACE", "Pause"), ("V", "Voice on")])

    # Footer
    s.footer("[ ] sensitivity  - = gain  tab help")
    s.save("record.png")


def mic_check_screen():
    s = Screen(TINT_MIC_CHECK)
    s.status_bar("MIC", MIC_CHECK, "8 recordings, 14.8 GB free", OK)
    s.band(MIC_CHECK, "MIC CHECK", s.levels_icon, right_text="Gain +12 dB", right_f=F0())

    # Live detector verdict
    s.circle(11, 52, 4, OK)
    s.t(20, 52, "Voice detected", F2(), TEXT, anchor="lm")

    # Meter row with scale below
    meter_y = 67
    s.level_meter(6, meter_y, 200, 10, level_db=-22.0, peak_db=-18.0, colour=OK, noise_db=-52.0, trigger_db=-40.0)
    s.t(234, meter_y + 5, "-22", F0(), TEXT, anchor="rm")

    # Scale ticks below meter (-60, -40, -20, 0)
    for db in range(-60, 1, 20):
        tx = s.meter_x(6, 200, db)
        s.t(tx, meter_y + 12, str(db), F0(), MUTED, anchor="ma")

    # Numbers behind markers
    s.t(6, meter_y + 23, "noise -52 dB, trigger -40 dB", F0(), MUTED, anchor="la")

    # Key row (top = 101)
    s.key_hints(101, [("ENTER", "Calibrate"), ("[ ]", "Sens 3"), ("- =", "Gain")])

    # Footer
    s.footer("s speaker  v voice  tab help  esc back")
    s.save("mic-check.png")


def player_screen():
    s = Screen(TINT_LIBRARY)
    s.status_bar("PLAY", LIBRARY, "8 recordings, 14.8 GB free", OK)
    s.band(LIBRARY, "PLAYING", s.play_icon, right_text="Volume 7", right_f=F0())

    # Middle area: Name & counter
    s.t(8, 48, "rec_0008", F9B(), TEXT, anchor="la")
    s.t(234, 48, "8 / 8", F0(), MUTED, anchor="ra")

    # Large position / length
    now_text = "00:42"
    s.t(8, 72, now_text, F12B(), TEXT, anchor="lm")
    now_w = s.d.textlength(now_text, font=F12B()) / S
    s.t(8 + now_w + 6, 73, "/ 02:15", F2(), MUTED, anchor="lm")

    # Progress bar (progress = 42 / 135 s = 0.31)
    s.progress_bar(10, 95, 220, 0.31, LIBRARY)

    # Key row (top = 101)
    s.key_hints(101, [("SPACE", "Pause"), (", /", "-+10 s"), ("D", "Delete")])

    # Footer
    s.footer("; . prev/next  - = volume  esc back")
    s.save("player.png")


if __name__ == "__main__":
    record_screen()
    mic_check_screen()
    player_screen()
    print(f"Rendered screenshots to {OUT}")
