"""Plots for the closed-loop blower simulation. Run from sim/: python3 plot.py
Palette: dataviz reference palette, light mode (slots 1 to 3 only, validated all-pairs)."""
import csv
import json
import os

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

SURFACE = "#fcfcfb"
INK = "#0b0b0b"
INK2 = "#52514e"
GRID = "#e6e5e0"
S1, S2, S3 = "#2a78d6", "#eb6834", "#1baf7a"
SHADE = "#d9d8d2"

plt.rcParams.update({
    "figure.facecolor": SURFACE, "axes.facecolor": SURFACE, "savefig.facecolor": SURFACE,
    "axes.edgecolor": GRID, "axes.labelcolor": INK2, "xtick.color": INK2, "ytick.color": INK2,
    "text.color": INK, "axes.grid": True, "grid.color": GRID, "grid.linewidth": 0.8,
    "axes.spines.top": False, "axes.spines.right": False, "font.size": 10, "lines.linewidth": 2,
    "legend.frameon": False,
})

OUT = "out"


def load(name):
    with open(os.path.join(OUT, name + ".csv")) as f:
        rows = list(csv.DictReader(f))
    cols = {k: [] for k in rows[0]}
    for r in rows:
        for k, v in r.items():
            try:
                cols[k].append(float(v))
            except ValueError:
                cols[k].append(v)
    return cols


def events(name):
    with open(os.path.join(OUT, name + "_events.csv")) as f:
        return list(csv.DictReader(f))


def hours(c):
    return [t / 3600.0 for t in c["t_s"]]


def shade_states(ax, c, h):
    """Shade LID_OPEN and RECOVERY spans, and wind, in one neutral tone with text labels (never color alone)."""
    spans = []
    cur, start = None, None
    for x, st in zip(h, c["state"]):
        tag = st if st in ("LID_OPEN", "RECOVERY") else None
        if tag != cur:
            if cur:
                spans.append((cur, start, x))
            cur, start = tag, x
    for tag, a, b in spans:
        ax.axvspan(a, b, color=SHADE, alpha=0.6 if tag == "LID_OPEN" else 0.3, lw=0)
    w = [x for x, v in zip(h, c["wind"]) if v > 1.0]
    if w:
        ax.axvspan(min(w), max(w), facecolor=SHADE, alpha=0.25, lw=0, hatch="//", edgecolor=GRID)


def rolling(x, n):
    out, acc, q = [], 0.0, []
    for v in x:
        q.append(v)
        acc += v
        if len(q) > n:
            acc -= q.pop(0)
        out.append(acc / len(q))
    return out


def overview(name, title):
    c = load(name)
    h = hours(c)
    ev = events(name)
    fig, axs = plt.subplots(3, 1, figsize=(11, 8.2), sharex=True, gridspec_kw={"height_ratios": [2.2, 1.2, 1]})
    ax = axs[0]
    shade_states(ax, c, h)
    ax.plot(h, c["setpoint_c"], color=INK2, lw=1.2, ls=(0, (4, 3)), label="Setpoint")
    ax.plot(h, c["chamber_c"], color=S1, label="Chamber (true)")
    ax.plot(h, c["probe_c"], color=S2, lw=1.0, alpha=0.85, label="Pit probe (filtered)")
    ax.set_ylabel("Temperature (C)")
    ax.set_ylim(0, max(max(c["chamber_c"]) + 10, 140))
    ax.legend(loc="lower left", ncol=3)
    ax.set_title(title, loc="left", fontsize=12, color=INK)
    labelled = set()
    offsets = [(-70, -30), (10, -48), (-120, -62), (20, 20)]
    k = 0
    for e in ev:
        t = float(e["t_s"]) / 3600.0
        if e["kind"] == "fault":
            ax.annotate(e["value"], (t, float(e["pit_c"])), xytext=(-20, 30), textcoords="offset points",
                        fontsize=9, color=INK, arrowprops=dict(arrowstyle="-", color=INK2, lw=0.8))
        if e["kind"] == "warnings" and e["value"] not in ("0x00",) and e["value"] not in labelled:
            mask = int(e["value"], 16)
            names = [n for b, n in [(1, "PIT_HIGH"), (2, "PIT_LOW"), (4, "FUEL_LOW"), (8, "CREEP")] if mask & b]
            ax.annotate(" + ".join(names), (t, float(e["pit_c"])), xytext=offsets[k % len(offsets)], textcoords="offset points",
                        fontsize=9, color=INK, arrowprops=dict(arrowstyle="-", color=INK2, lw=0.8))
            labelled.add(e["value"])
            k += 1
    for e in ev:
        if e["kind"] == "state" and e["value"] == "LID_OPEN":
            t = float(e["t_s"]) / 3600.0
            ax.text(t + 0.05, ax.get_ylim()[1] - 8, "lid", fontsize=9, color=INK2)
    w = [x for x, v in zip(h, c["wind"]) if v > 1.0]
    if w:
        ax.text(min(w) + 0.02, ax.get_ylim()[1] - 8, "wind x2", fontsize=9, color=INK2)

    ax = axs[1]
    shade_states(ax, c, h)
    ax.plot(h, rolling(c["duty_cmd"], 12), color=S1, label="Commanded duty, 1 min avg")
    ax.plot(h, rolling(c["duty_applied_avg"], 12), color=S3, lw=1.2, label="Applied duty (after long-PWM), 1 min avg")
    ax.set_ylabel("Blower duty (%)")
    ax.set_ylim(0, 105)
    ax.legend(loc="upper right", ncol=2)

    ax = axs[2]
    ax.plot(h, c["fuel_left_kg"], color=S1, label="Charcoal left")
    ax.plot(h, c["lit_kg"], color=S2, label="Lit charcoal")
    ax.set_ylabel("kg")
    ax.set_xlabel("Hours from start")
    ax.legend(loc="upper right", ncol=2)
    fig.tight_layout()
    fig.savefig(os.path.join(OUT, f"{name}_overview.png"), dpi=130)
    plt.close(fig)


def blower_zoom(name):
    with open(os.path.join(OUT, name + "_fast.csv")) as f:
        rows = list(csv.DictReader(f))
    t = [(float(r["t_s"]) - float(rows[0]["t_s"])) for r in rows]
    fig, axs = plt.subplots(2, 1, figsize=(11, 5.6), sharex=True)
    ax = axs[0]
    ax.plot(t, [float(r["duty_cmd"]) for r in rows], color=S1, label="Commanded duty (PID)")
    ax.step(t, [float(r["duty_applied"]) for r in rows], color=S2, lw=1.2, where="post",
            label="Applied PWM duty (long-PWM + 300 ms kick)")
    ax.set_ylabel("Duty (%)")
    ax.set_ylim(0, 65)
    ax.legend(loc="upper right", ncol=2)
    ax.set_title("Blower in HOLD, 10 min at 2 h: PID wants less than the 30% min-start, so the HAL pulses", loc="left", fontsize=12)
    ax = axs[1]
    ax.plot(t, [float(r["air_cfm"]) for r in rows], color=S3, label="Air to the coals (CFM)")
    ax.set_ylabel("CFM")
    ax.set_xlabel("Seconds")
    ax.legend(loc="upper right")
    fig.tight_layout()
    fig.savefig(os.path.join(OUT, "blower_zoom.png"), dpi=130)
    plt.close(fig)


def compare(names, labels, title, fname, t_max_h=None, series="chamber_c"):
    fig, ax = plt.subplots(figsize=(11, 4.6))
    cols = [S1, S2, S3]
    sp = None
    for n, lab, col in zip(names, labels, cols):
        c = load(n)
        h = hours(c)
        if t_max_h:
            k = sum(1 for x in h if x <= t_max_h)
            h, y, sp = h[:k], c[series][:k], c["setpoint_c"][:k]
        else:
            y, sp = c[series], c["setpoint_c"]
        ax.plot(h, y, color=col, label=lab)
    ax.plot(h, sp, color=INK2, lw=1.2, ls=(0, (4, 3)), label="Setpoint")
    ax.set_ylabel("Chamber temperature (C)")
    ax.set_xlabel("Hours from start")
    ax.set_title(title, loc="left", fontsize=12)
    ax.legend(loc="lower right", ncol=4)
    fig.tight_layout()
    fig.savefig(os.path.join(OUT, fname), dpi=130)
    plt.close(fig)


if __name__ == "__main__":
    overview("nominal", "Nominal 12 h cook: firmware defaults, 10 CFM blower, flap, 4 kg basket (simulated)")
    overview("no_flap_93c", "No backdraft flap, 93 C setpoint: stack draft alone overheats the pit (simulated)")
    blower_zoom("nominal")
    compare(["nominal", "caps_removed", "fan_20cfm"],
            ["Startup cap 50% (current)", "Caps removed (Phase 1 defaults)", "20 CFM blower, current caps"],
            "Startup: the cap removes most of the overshoot (simulated)", "startup_compare.png", t_max_h=1.5)
    compare(["flap_93c", "no_flap_93c"], ["Flap installed", "No flap"],
            "93 C setpoint with and without the backdraft flap (simulated)", "flap_compare.png")
    print("plots written to", OUT)
