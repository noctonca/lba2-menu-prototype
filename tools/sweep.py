#!/usr/bin/env python3
"""Visit every row of every Options page and every chip of the Controls page at
several screen sizes and switch settings, and list what does not fit: help cut
off, rows running into the help or off the canvas, labels running into their
values or out of their row, key names cut in their chips; and what is wrong on
a screen: not exactly one cursor, a red key the help does not explain, a clash
only one of the two actions shows, and the screen rules of docs/UX.md (edge,
margin, trailing, pitch, current, value, proximity). The self test then walks the
menu by its keys and lists what they do wrong (see README "Testing").

    tools/sweep.py [--bin build/lba2menu] [--sizes 640x480,1920x1080] [--jobs N]
                   [--combo band=1,gameplay=0 ...] [--only PAGE] [--again]
                   [--layout-only] [--scripts [--shots DIR]] [--check-again]

By default each (size, combo) is one `lba2menu --selftest` process, and --jobs
of them run at once. --again prints, under each problem, the command line that
opens the screen it was found on (add --headless and `; shot x.png` to the --do
list for a picture). --only keeps one page ("Keys and buttons", "Sound" ...).

--layout-only leaves out the walks by keys. --scripts walks the same screens as
--layout-only from outside instead: a script per run, read back through `state`
lines and checked here. Slower; it is the cross-check that the self test and the
script path agree (the two must list the same problems), and the only way to
take --shots.

--check-again runs every problem's command line headless and checks that the
screen it opens has the problem; for a walk's problem, that it ends on the page
the problem names.
"""
import argparse, json, os, shlex, subprocess, sys
from concurrent.futures import ThreadPoolExecutor

PAGES = ["Options", "Preset", "Language and interface", "Display and graphics", "Sound",
         "Controls and camera", "Advanced and reset", "Gameplay"]

# Actions per group on the Controls page (controls.cpp kActions).
CONTROL_GROUPS = [5, 6, 4, 7, 4, 4, 6]

# The same eight as selftest.cpp kCombos.
COMBOS = ["band=1,gameplay=0", "band=1,gameplay=1", "band=0,gameplay=1", "help640=1,gameplay=1",
          "devices=0", "padnames=2", "padnames=3", "chip=1"]

def parse_combo(spec):
    return {k: int(v) for k, v in (part.split("=") for part in spec.split(","))}

# ───────────────────────────── The self test ─────────────────────────────

def run_selftest(binary, size, combo, only, layout_only):
    """Problems as {line: again-command} from one --selftest process."""
    cmd = [binary, "--selftest", "--size", size, "--combo", combo]
    if only:
        cmd += ["--only", only]
    if layout_only:
        cmd += ["--layout-only"]
    p = subprocess.run(cmd, capture_output=True, text=True, timeout=300)
    if p.returncode not in (0, 1):
        raise SystemExit(f"{' '.join(cmd)} failed ({p.returncode}):\n{p.stderr}")
    found, last = {}, None
    for line in p.stdout.splitlines():
        if line.startswith("    again: "):
            found[last] = line[len("    again: "):]
        elif not line.endswith("checks"):
            last = line
            found[line] = None
    return found

# ───────────────────────────── The script path ─────────────────────────────

def controls_lines(columns_kb, columns_pad):
    """Every chip of every group, on each device the view shows."""
    lines = ["open Keys and buttons", "wait 1", "state"]
    devices = [columns_kb] if columns_pad is None else [columns_kb, columns_pad]
    for d, cols in enumerate(devices):
        if d:
            lines += ["key tab", "wait 1"]
        for g, count in enumerate(CONTROL_GROUPS):
            lines += ["wait 1", "state", "key enter", "wait 1"]
            for r in range(count):
                for c in range(cols):
                    lines += ["wait 1", "state"]
                    if c + 1 < cols:
                        lines += ["key right"]
                lines += ["key left"] * (cols - 1) + ["key down"]
            lines += ["key esc", "wait 1", "key down"]
    return lines

def run_scripts(binary, size, combo, only, shots):
    switches = parse_combo(combo)
    tag = f"{size}-{'-'.join(f'{k}{v}' for k, v in switches.items())}"
    lines = [f"switch {k} {v}" for k, v in switches.items()] + ["switch typing 2", "wait 1"]
    if not only or only == "Keys and buttons":
        lines += controls_lines(2, 1) if switches.get("devices", 1) else controls_lines(3, None)
        if shots:
            lines += [f"shot {shots}/{tag}-Keys_and_buttons.png"]
    for page in PAGES:
        if page == "Gameplay" and not switches.get("gameplay"):
            continue
        if only and only != page:
            continue
        lines += [f"open {page}", "wait 1", "state"]
        for i in range(1, 12):
            lines += ["key down", "wait 1", "state"]
        if shots:
            lines += [f"shot {shots}/{tag}-{page.replace(' ', '_')}.png"]
    lines.append("quit")
    p = subprocess.run([binary, "--headless", "--size", size, "--script", "/dev/stdin"],
                       input="\n".join(lines) + "\n", capture_output=True, text=True, timeout=300)
    found = {}
    label = json.dumps(switches)
    for l in p.stdout.splitlines():
        if l.startswith("state "):
            st = json.loads(l[6:])
            for prob in check(st):
                found[f"{size} {label} [{st['page']}] {prob}"] = None
    return found

def check(state):
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
    controls = state.get("controls", {})
    chips = controls.get("chips", [])
    for k in chips:
        if not k["fits"]:
            problems.append(f"key name cut in its chip: {k['action']!r} {k['column']} {k['name']!r}")
    # One cursor: exactly one row or chip wears it.
    if state["rows"] or chips:
        cursors = sum(r["fill"] == "cursor" for r in state["rows"])
        cursors += sum(k["fill"] in ("cursor", "waiting") for k in chips)
        if cursors != 1:
            problems.append(f"{cursors} cursors on the screen")
    # A red chip under the cursor says in the help what else has its key.
    for k in chips:
        if k["fill"] == "cursor" and k["conflict"] and " is also " not in v["text"]:
            problems.append(f"red key not explained in the help: {k['action']!r} {k['column']} {k['name']!r}")
    # A clash goes both ways: the other action shows one too.
    clashing = {a for a, _, _ in controls.get("conflicts", [])}
    for a, col, other in controls.get("conflicts", []):
        if other not in clashing:
            problems.append(f"clash one way: {a!r} {col} is also {other!r}, which shows no clash")
    # The rows by pane, in draw order (the first-launch title is not a pane).
    panes = {}
    for r in state["rows"]:
        if r["pane"] != "title":
            panes.setdefault(r["pane"], []).append(r)
    wide = state["layout"] == "wide"
    # One edge per pane: its rows share x0 and x1; wide labels start 16 in.
    for pane, rows in panes.items():
        px0, px1 = rows[0]["rect"][0], rows[0]["rect"][2]
        for r in rows:
            x0, x1 = r["rect"][0], r["rect"][2]
            if (x0, x1) != (px0, px1):
                problems.append(f"edge: {r['label']!r} at x {x0}..{x1}, its pane's rows at {px0}..{px1}")
            if wide and pane not in ("column", "confirm") and r["text_left"] != x0 + 16:
                problems.append(f"edge: {r['label']!r} starts at x {r['text_left']}, not {x0 + 16}")
    # The Controls page's first column head (the action names') starts on its rows' label edge.
    heads = controls.get("heads", [])
    if wide and heads:
        h = heads[0]
        for r in state["rows"]:
            if r["rect"][0] <= h["rect"][0] <= r["rect"][2] and r["rect"][1] > h["rect"][3]:
                if h["rect"][0] != r["rect"][0] + 16:
                    problems.append(f"edge: head {h['text']!r} starts at x {h['rect'][0]}, not {r['rect'][0] + 16}")
                break
    # Margins: nothing on a wide page within 32 of the sides or 24 of the top and bottom.
    if wide:
        boxes = [(r["label"], r["rect"]) for r in state["rows"]]
        boxes += [(f"{k['action']} {k['column']}", k["rect"]) for k in chips]
        boxes += [(h["text"], h["rect"]) for h in controls.get("heads", [])]
        if v["where"] != "none":
            boxes.append(("help", v["rect"]))
        for what, (x0, y0, x1, y1) in boxes:
            if x0 < 32 or y0 < 24 or x1 > W - 33 or y1 > H - 25:
                problems.append(f"margin: {what!r} [{x0}, {y0}, {x1}, {y1}] in the margin")
    # One trailing edge: wide, what a row shows on the right (a value, a slider, steps, its
    # last key chip) ends on the column 24 in from the row's right edge.
    if wide:
        for r in state["rows"]:
            x1 = r["rect"][2]
            if r["value_right"] > 0 and r["value_right"] != x1 - 24:
                problems.append(f"trailing: {r['label']!r} ends at x {r['value_right']}, not {x1 - 24}")
    # The pitch: in a pane, each row starts 56 below the one above and is 50 tall.
    for pane, rows in panes.items():
        if len(rows) < 2:
            continue
        for above, r in zip(rows, rows[1:]):
            d = r["rect"][1] - above["rect"][1]
            if d != 56:
                problems.append(f"pitch: {r['label']!r} starts {d} below the row above, not 56")
        for r in rows:
            h = r["rect"][3] - r["rect"][1]
            if h != 50:
                problems.append(f"pitch: {r['label']!r} is {h} tall, not 50")
    # At most one current-value plasma in a pane.
    for pane, rows in panes.items():
        n = sum(r["fill"] == "current" for r in rows)
        if n > 1:
            problems.append(f"current: {n} current-value rows in the {pane} pane")
    # Every setting row shows its value.
    for r in state["rows"]:
        if r["setting"] and not r["value"]:
            problems.append(f"value: setting {r['label']!r} shows no value")
    # Proximity: a pane's last row is further from the help below than rows are apart (5).
    if v["where"] != "none":
        for pane, rows in panes.items():
            gap = hy0 - max(r["rect"][3] for r in rows) - 1
            if 0 <= gap <= 5:
                problems.append(f"proximity: {gap} px between {pane} and the {v['where']}, rows are 5 apart")
    # Proximity: a column head is closer to the rows below it than to the rows above it.
    for h in controls.get("heads", []):
        hx0, hy0_, _, hy1_ = h["rect"]
        below = [r["rect"][1] - hy1_ - 1 for r in state["rows"]
                 if r["rect"][0] <= hx0 <= r["rect"][2] and r["rect"][1] > hy1_]
        above = [hy0_ - r["rect"][3] - 1 for r in state["rows"]
                 if r["rect"][0] <= hx0 <= r["rect"][2] and r["rect"][3] < hy0_]
        if below and above and min(below) >= min(above):
            problems.append(f"proximity: head {h['text']!r} is {min(below)} px above its rows, "
                            f"{min(above)} px below the row above")
    return problems

# ───────────────────────────── Checking the command lines ─────────────────────────────

# The walks by keys name themselves at the start of each problem they find.
WALKS = {"esc", "reach", "setting", "preset", "bind", "walk"}

def check_again(line, again):
    """None when the command line opens a screen with this problem, else what it showed."""
    if not again:
        return "no command line"
    args = shlex.split(again)
    i = args.index("--do")
    args[i + 1] += "; wait 1; state"
    p = subprocess.run(args[:1] + ["--headless"] + args[1:], capture_output=True, text=True, timeout=60)
    states = [json.loads(l[6:]) for l in p.stdout.splitlines() if l.startswith("state ")]
    if not states:
        return f"no state ({p.returncode}): {p.stderr.strip()[-200:]}"
    st = states[-1]
    page = line.split(" [", 1)[1].split("] ", 1)[0]
    problem = line.split("] ", 1)[1]
    if st["page"] != page:
        return f"on page {st['page']!r}"
    if problem.startswith("walk did not reach") or problem.split(":")[0] in WALKS:
        return None  # a walk by keys: what it saw is not in one state, the page is
    if problem not in check(st):
        return "problem not on that screen"
    return None

def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--bin", default=os.path.join(os.path.dirname(__file__), "..", "build", "lba2menu"))
    ap.add_argument("--sizes", default="640x480,1280x720,1280x800,1920x1080,2400x1080")
    ap.add_argument("--combo", action="append", help="name=v,name=v (repeat; default: the eight)")
    ap.add_argument("--only", help="one page by name")
    ap.add_argument("--again", action="store_true", help="print the command line that shows each problem")
    ap.add_argument("--layout-only", action="store_true", help="the layout walk only, no walks by keys")
    ap.add_argument("--scripts", action="store_true", help="walk through scripts instead of --selftest")
    ap.add_argument("--shots", help="with --scripts: a PNG of each page")
    ap.add_argument("--check-again", action="store_true", help="replay every problem's command line")
    ap.add_argument("--jobs", type=int, default=os.cpu_count() or 1)
    a = ap.parse_args()
    if a.shots and not a.scripts:
        ap.error("--shots needs --scripts")
    if a.check_again and a.scripts:
        ap.error("--check-again needs the self test (no --scripts)")
    if a.layout_only and a.scripts:
        ap.error("--scripts walks the layout only already")
    if a.shots:
        os.makedirs(a.shots, exist_ok=True)
    combos = a.combo or COMBOS
    jobs = [(size, c) for size in a.sizes.split(",") for c in combos]
    if a.scripts:
        work = lambda j: run_scripts(a.bin, j[0], j[1], a.only, a.shots)
    else:
        work = lambda j: run_selftest(a.bin, j[0], j[1], a.only, a.layout_only)
    found = {}
    with ThreadPoolExecutor(max_workers=max(1, a.jobs)) as pool:
        for result in pool.map(work, jobs):
            for line, again in result.items():
                found.setdefault(line, again)
    for line in sorted(found):
        print(line)
        if a.again and found[line]:
            print(f"    again: {found[line]}")
    print(f"{len(found)} problem(s)")
    if a.check_again:
        items = sorted(found.items())
        with ThreadPoolExecutor(max_workers=max(1, a.jobs)) as pool:
            verdicts = list(pool.map(lambda kv: check_again(*kv), items))
        bad = [(line, v) for (line, _), v in zip(items, verdicts) if v]
        for line, v in bad:
            print(f"again does not show it ({v}): {line}")
        print(f"{len(items) - len(bad)} of {len(items)} command lines show their problem")
        if bad:
            return 2
    return 1 if found else 0

if __name__ == "__main__":
    sys.exit(main())
