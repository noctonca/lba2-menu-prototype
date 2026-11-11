#!/usr/bin/env python3
"""Record what a build does, and compare two records: the check for a change
meant to move code without changing what a player sees.

    tools/baseline.py record OUTDIR [--bin build/lba2menu] [--scripts FILE] [--jobs N]
    tools/baseline.py diff A B [--ignore key,key] [--rename old=new,...] [--max N] [--context N]
    tools/baseline.py scripts [--bin build/lba2menu] [--out FILE] [--jobs N]

record runs 28 scripts headless under a fixed clock: at each of four screen
sizes, six seeded random walks of 400 keys (now and then a pad button, a size
change or an `open`) with a `state` after every key and a picture every 25
keys, and the sweep's layout walk with a picture per page. Each run leaves
<name>.script (what was run), <name>.states (one JSON per line), <name>.trace
and <name>.shots (a sha256 per picture, in order). The game writes its text in
code page 850; .script and .states are kept in UTF-8.

The scripts are made once, from a build, into .runs/baseline/scripts.json
(record makes them when the file is missing). A walk never quits: after an Esc,
the next Esc or Enter is an Enter, and an Esc that still closes the menu (on
the closed note) is made an Enter, until no run quits on that build.

diff prints, per run, the first state that differs, field by field, and which
pictures differ; exit 1 if any run differs. --ignore leaves out fields with
that name at any depth; --rename compares A's field old (its path from the
top, as `bindings` or `bindings.heads`) as if it were new, for a state field
renamed on purpose. --context N prints the N keys before the first
difference with the page and row each left, from A and B.

    tools/baseline.py record .runs/before          # on the commit before
    (change the code, build)
    tools/baseline.py record .runs/after
    tools/baseline.py diff .runs/before .runs/after --context 6
"""
import argparse, hashlib, json, os, random, subprocess, sys, tempfile
from concurrent.futures import ThreadPoolExecutor

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
sys.path.insert(0, os.path.join(ROOT, "tools"))
import sweep  # noqa: E402  (its page list and layout walk)

DEFAULT_BIN = os.path.join(ROOT, "build", "lba2menu")
DEFAULT_SCRIPTS = os.path.join(ROOT, ".runs", "baseline", "scripts.json")

SIZES = ["640x480", "1024x480", "1280x720", "1920x1080"]
SEEDS = range(6)
STEPS = 400
SHOT = "shot {SHOT}"

KEYS = (["up"] * 14 + ["down"] * 18 + ["left"] * 8 + ["right"] * 10 + ["enter"] * 12 + ["esc"] * 8
        + ["tab"] * 2 + ["del"] * 2 + ["pgup"] * 2 + ["pgdn"] * 2 + ["f2"] * 1 + ["f11"] * 1)
RAW = ["J", "K", "Left Alt", "pad:a", "pad:start", "pad:dpad-up", "pad:lb"]
OPENS = ["Options", "Preset", "Sound", "Controls and camera", "Advanced and reset", "Keys and buttons",
         "More options", "Display and graphics", "Language and interface"]


def walk_lines(seed):
    rnd = random.Random(seed)
    lines = []
    need_enter = False  # the last of Esc and Enter was an Esc: the next one is an Enter
    for step in range(STEPS):
        r = rnd.random()
        if r < 0.04:
            lines.append(f"press {rnd.choice(RAW)}")
        elif r < 0.055:
            lines.append(f"size {rnd.choice(SIZES)}")
        elif r < 0.07 and not need_enter:
            lines.append(f"open {rnd.choice(OPENS)}")
        else:
            k = rnd.choice(KEYS)
            if k == "esc" and need_enter:
                k = "enter"
            if k == "esc":
                need_enter = True
            elif k == "enter":
                need_enter = False
            lines.append(f"key {k}")
        lines += ["wait 1", "state"]
        if step % 25 == 24:
            lines.append(SHOT)
    return lines


def jobs():
    """(name, size, first launch, lines) for each run."""
    out = []
    for size in SIZES:
        for seed in SEEDS:
            pre = ["switch whole 1"] if seed % 2 else []
            if seed == 3:
                pre.append("set text_lang 1")
            out.append((f"walk-s{seed}-{size}", size, seed == 5,
                        pre + walk_lines(seed * 1000 + SIZES.index(size))))
        lines = ["switch whole 1", "wait 1"] + sweep.bindings_lines() + [SHOT]
        for page in sweep.PAGES:
            lines += [f"open {page}", "wait 1", "state"]
            for _ in range(1, 12):
                lines += ["key down", "wait 1", "state"]
            lines.append(SHOT)
        lines += sweep.more_options_lines() + [SHOT]
        out.append((f"layout-{size}", size, False, lines))
    return out


def run_one(binary, outdir, job):
    name, size, first, lines = job
    with tempfile.TemporaryDirectory() as tmp:
        shots = []
        body = []
        for l in lines:
            if l == SHOT:
                p = os.path.join(tmp, f"{len(shots):04d}.png")
                shots.append(p)
                l = f"shot {p}"
            body.append(l)
        body.append("quit")
        trace = os.path.join(outdir, name + ".trace")
        cmd = [binary, "--headless", "--fixed-dt", "16", "--size", size, "--trace", trace,
               "--script", "/dev/stdin"] + (["--first"] if first else [])
        p = subprocess.run(cmd, input="\n".join(body) + "\n", capture_output=True, text=True,
                           encoding="cp850", timeout=600)
        with open(os.path.join(outdir, name + ".script"), "w", encoding="utf-8") as f:
            f.write(json.dumps({"size": size, "first": first}) + "\n")
            f.write("\n".join(lines) + "\n")
        with open(os.path.join(outdir, name + ".states"), "w", encoding="utf-8") as f:
            for l in p.stdout.splitlines():
                if l.startswith("state "):
                    f.write(l[6:] + "\n")
                elif l.startswith("err"):
                    f.write(json.dumps({"err": l}) + "\n")
        with open(os.path.join(outdir, name + ".shots"), "w") as f:
            for s in shots:
                f.write((hashlib.sha256(open(s, "rb").read()).hexdigest() if os.path.exists(s) else "missing") + "\n")
        return name, p.returncode


def fix_quits(binary, job):
    """Make each Esc that ends a walk (the closed note's) an Enter, until none does."""
    name, size, first, lines = job
    lines = list(lines)
    for _ in range(200):
        with tempfile.TemporaryDirectory() as tmp:
            run_one(binary, tmp, (name, size, first, lines))
            trace = open(os.path.join(tmp, name + ".trace"), encoding="cp850").read().splitlines()
        quits = [i for i, l in enumerate(trace) if l.endswith(" quit") and " cmd " not in l]
        if not quits:
            return (name, size, first, lines)
        idx = sum(1 for l in trace[: quits[0]] if " cmd " in l) - 1
        assert lines[idx] == "key esc", (name, idx, lines[idx])
        lines[idx] = "key enter"
    raise SystemExit(f"{name}: still quits after 200 Esc made Enter")


def make_scripts(binary, out, n):
    js = jobs()
    with ThreadPoolExecutor(max_workers=n) as pool:
        fixed = list(pool.map(lambda j: fix_quits(binary, j), js))
    os.makedirs(os.path.dirname(os.path.abspath(out)), exist_ok=True)
    with open(out, "w") as f:
        json.dump({"bin": binary, "jobs": fixed}, f)
    changed = sum(x[3] != y[3] for x, y in zip(js, fixed))
    print(f"{len(fixed)} scripts made with {binary} ({changed} with an Esc made Enter), in {out}")


def cmd_scripts(a):
    make_scripts(os.path.abspath(a.bin), a.out, a.jobs)


def cmd_record(a):
    binary = os.path.abspath(a.bin)
    if not os.path.exists(a.scripts):
        make_scripts(binary, a.scripts, a.jobs)
    js = [tuple(j) for j in json.load(open(a.scripts))["jobs"]]
    os.makedirs(a.outdir, exist_ok=True)
    failed = 0
    with ThreadPoolExecutor(max_workers=a.jobs) as pool:
        for name, rc in pool.map(lambda j: run_one(binary, a.outdir, j), js):
            if rc:
                failed += 1
                print(f"{name}: exit {rc}")
    print(f"{len(js)} runs of {binary} in {a.outdir}")
    return 1 if failed else 0


def flat(d, prefix=""):
    if isinstance(d, dict):
        for k, v in d.items():
            yield from flat(v, f"{prefix}.{k}" if prefix else k)
    elif isinstance(d, list):
        for i, v in enumerate(d):
            yield from flat(v, f"{prefix}[{i}]")
    else:
        yield prefix, d


def renamed(path, renames):
    """The path with a renamed field's path (from the top) put in its new name's place."""
    for old, new in renames.items():
        if path == old or path.startswith((old + ".", old + "[")):
            return new + path[len(old):]
    return path


def read_lines(path):
    return open(path, encoding="utf-8").read().splitlines() if os.path.exists(path) else []


def summary(state):
    if "err" in state:
        return state["err"]
    rows = [r["label"] for r in state.get("rows", [])]
    return f"{state.get('layout')}/{state.get('page')} cursor {state.get('cursor')} {rows}"


def context(a, n, i, back):
    """The keys before state #i of run n, with the page each left, from A and B."""
    lines = read_lines(os.path.join(a.A, n + ".script"))[1:]
    at = [k for k, l in enumerate(lines) if l == "state"]
    if i >= len(at):
        return
    sa = read_lines(os.path.join(a.A, n + ".states"))
    sb = read_lines(os.path.join(a.B, n + ".states"))
    lo = at[i - back] + 1 if i - back >= 0 else 0
    for k in range(lo, at[i] + 1):
        l = lines[k]
        if l == "state":
            j = at.index(k)
            x = summary(json.loads(sa[j])) if j < len(sa) else "-"
            y = summary(json.loads(sb[j])) if j < len(sb) else "-"
            print(f"       #{j} A {x}")
            if y != x:
                print(f"       #{j} B {y}")
        elif l not in ("wait 1", SHOT):
            print(f"     > {l}")


def cmd_diff(a):
    ignore = set(filter(None, (a.ignore or "").split(",")))
    renames = dict(r.split("=", 1) for r in filter(None, (a.rename or "").split(",")))
    names = sorted(n[: -len(".states")] for n in os.listdir(a.A) if n.endswith(".states"))
    same = differ = states = 0
    for n in names:
        sa = read_lines(os.path.join(a.A, n + ".states"))
        sb = read_lines(os.path.join(a.B, n + ".states"))
        states += len(sa)
        first = None
        for i, (x, y) in enumerate(zip(sa, sb)):
            if x == y:
                continue
            fx = {renamed(k, renames): v for k, v in flat(json.loads(x))}
            fy = dict(flat(json.loads(y)))
            keys = [k for k in sorted(set(fx) | set(fy)) if fx.get(k) != fy.get(k)
                    and not ignore & {p.split("[")[0] for p in k.split(".")}]
            if keys:
                first = (i, keys, fx, fy)
                break
        if first is None and len(sa) != len(sb):
            first = (min(len(sa), len(sb)), ["<states>"], {"<states>": len(sa)}, {"<states>": len(sb)})
        ha = read_lines(os.path.join(a.A, n + ".shots"))
        hb = read_lines(os.path.join(a.B, n + ".shots"))
        shots = [i for i, (x, y) in enumerate(zip(ha, hb)) if x != y] + (["count"] if len(ha) != len(hb) else [])
        if first is None and not shots:
            same += 1
            continue
        differ += 1
        print(f"== {n}")
        if first:
            i, keys, fx, fy = first
            print(f"   first differing state #{i} ({len(keys)} fields)")
            for k in keys[: a.max]:
                print(f"     {k}: {fx.get(k)!r} -> {fy.get(k)!r}")
            if a.context:
                context(a, n, i, a.context)
        if shots:
            print(f"   pictures differ: {shots[:10]}")
    print(f"{same} runs the same, {differ} differ ({states} states compared)")
    return 1 if differ else 0


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    r = sub.add_parser("record")
    r.add_argument("outdir")
    r.add_argument("--bin", default=DEFAULT_BIN)
    r.add_argument("--scripts", default=DEFAULT_SCRIPTS)
    r.add_argument("--jobs", type=int, default=os.cpu_count() or 4)
    s = sub.add_parser("scripts")
    s.add_argument("--bin", default=DEFAULT_BIN)
    s.add_argument("--out", default=DEFAULT_SCRIPTS)
    s.add_argument("--jobs", type=int, default=os.cpu_count() or 4)
    d = sub.add_parser("diff")
    d.add_argument("A")
    d.add_argument("B")
    d.add_argument("--ignore")
    d.add_argument("--rename")
    d.add_argument("--max", type=int, default=12)
    d.add_argument("--context", type=int, default=0)
    a = ap.parse_args()
    return {"record": cmd_record, "diff": cmd_diff, "scripts": cmd_scripts}[a.cmd](a)


if __name__ == "__main__":
    sys.exit(main())
