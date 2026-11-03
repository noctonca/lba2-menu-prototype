#!/usr/bin/env python3
"""Visit every row of every Options page at several screen sizes and switch
settings, and list what does not fit: help cut off, rows running into the help
or off the canvas, labels running into their values or out of their row.

    tools/sweep.py [--bin build/lba2menu] [--sizes 640x480,1920x1080] [--shots DIR]
"""
import argparse, json, os, subprocess, sys

PAGES = ["Options", "Preset", "Language and interface", "Display and graphics", "Sound",
         "Controls and camera", "Advanced and reset", "Gameplay"]

def run(binary, size, switches, shots):
    lines = [f"switch {k} {v}" for k, v in switches.items()] + ["switch typing 2", "wait 1"]
    plan = []
    for page in PAGES:
        if page == "Gameplay" and not switches.get("gameplay"):
            continue
        lines += [f"open {page}", "wait 1", "state"]
        plan.append((page, 0))
        for i in range(1, 12):
            lines += ["key down", "wait 1", "state"]
            plan.append((page, i))
        if shots:
            tag = f"{size}-{'-'.join(f'{k}{v}' for k, v in switches.items())}-{page.replace(' ', '_')}"
            lines += [f"shot {shots}/{tag}.png"]
    lines.append("quit")
    script = "\n".join(lines) + "\n"
    p = subprocess.run([binary, "--headless", "--size", size, "--script", "/dev/stdin"],
                       input=script, capture_output=True, text=True, timeout=300)
    states = [json.loads(l[6:]) for l in p.stdout.splitlines() if l.startswith("state ")]
    return states

def check(state, where):
    problems = []
    W, H = state["logical"]
    v = state["voice"]
    if v.get("cut_off"):
        problems.append(f"help cut off ({v['lines_needed']} lines in {v['max_lines']}, {v['width']} px): {v['text']!r}")
    hx0, hy0, hx1, hy1 = v["rect"]
    for r in state["rows"]:
        x0, y0, x1, y1 = r["rect"]
        if y1 >= H or x1 >= W or x0 < 0:
            problems.append(f"row off the canvas: {r['label']!r} {r['rect']}")
        if v["where"] != "none" and y1 >= hy0 and y0 <= hy1 and x1 >= hx0 and x0 <= hx1:
            problems.append(f"row overlaps the {v['where']} by {y1 - hy0 + 1} px: {r['label']!r}")
        if r["text_right"] > x1:
            problems.append(f"text spills {r['text_right'] - x1} px: {r['label']!r} {r['value']!r}")
    return problems

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--bin", default=os.path.join(os.path.dirname(__file__), "..", "build", "lba2menu"))
    ap.add_argument("--sizes", default="640x480,1280x720,1280x800,1920x1080,2400x1080")
    ap.add_argument("--shots")
    a = ap.parse_args()
    if a.shots:
        os.makedirs(a.shots, exist_ok=True)
    combos = [{"band": 1, "gameplay": 0}, {"band": 1, "gameplay": 1}, {"band": 0, "gameplay": 1},
              {"help640": 1, "gameplay": 1}]
    found = {}
    for size in a.sizes.split(","):
        for sw in combos:
            for st in run(a.bin, size, sw, a.shots):
                for prob in check(st, size):
                    key = (size, json.dumps(sw), st["page"], prob)
                    found.setdefault(key, 0)
                    found[key] += 1
    for (size, sw, page, prob) in sorted(found):
        print(f"{size} {sw} [{page}] {prob}")
    print(f"{len(found)} problem(s)")
    return 1 if found else 0

if __name__ == "__main__":
    sys.exit(main())
