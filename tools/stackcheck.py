#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# (3.4.1) The stacks' worst case, from the built firmware: each function's frame (its pushes and its sp += -N),
# along every call path from a root. Calls through a pointer (call rN) are counted as the deepest of the functions
# whose address is taken (a table, a pointer: the engines, the sequencer's hooks, the store's flash steps), which
# over-estimates. Recursion is reported. Exit 1 when a root's worst case passes its stack.
#   tools/stackcheck.py build/fw/ram.elf LLVM_BIN
import re, struct, subprocess, sys

elf, tc = sys.argv[1], sys.argv[2]
dis = subprocess.run([f"{tc}/llvm-objdump", "-d", "--no-show-raw-insn", elf], capture_output=True, text=True).stdout
F, cur = {}, None
REGS = re.compile(r"r(\d+)(?:-r(\d+))?|rets|reti|psr|rete|retx")
def nregs(s):
    n = 0
    for m in REGS.finditer(s):
        if m.group(1) is not None:
            a, b = int(m.group(1)), int(m.group(2) or m.group(1)); n += abs(a - b) + 1
        else: n += 1
    return n
for line in dis.splitlines():
    m = re.match(r"^([0-9a-f]+) <([^>]+)>:", line)
    if m:
        cur = m.group(2); F[cur] = {"addr": int(m.group(1), 16), "frame": 0, "calls": set(), "ind": False}
        continue
    if cur is None: continue
    ins = re.sub(r"^\s*[0-9a-f]+:\s*", "", line).strip()
    m = re.match(r"\[--sp\] = \{(.*)\}", ins) or re.match(r"\[--sp\] = (\S+)$", ins)
    if m: F[cur]["frame"] += 4 * nregs(m.group(1)); continue
    m = re.match(r"sp \+= -(\d+)", ins)
    if m: F[cur]["frame"] += int(m.group(1)); continue
    m = re.match(r"(?:call|callns|goto)\s+.*<([^>+]+)(?:\+0x[0-9a-f]+)?>", ins)
    if m and m.group(1) != cur: F[cur]["calls"].add(m.group(1)); continue
    if re.match(r"(?:tbh|tbb)\b", ins): F[cur]["table"] = True
    if re.match(r"(?:call|callns)\s+r\d+", ins) and not F[cur].get("table"): F[cur]["ind"] = True
# the functions whose address sits in data or code as a word (a table) or is loaded into a register (a pointer)
byaddr = {v["addr"]: k for k, v in F.items()}
taken = set()
for line in dis.splitlines():
    for v in re.findall(r"= (\d{8})\b", line):
        if int(v) in byaddr: taken.add(byaddr[int(v)])
img = open(elf, "rb").read()
sh = subprocess.run([f"{tc}/llvm-readelf", "-S", elf], capture_output=True, text=True).stdout
for m in re.finditer(r"\]\s+(\S+)\s+PROGBITS\s+([0-9a-f]+)\s+([0-9a-f]+)\s+([0-9a-f]+)", sh):
    off, size = int(m.group(3), 16), int(m.group(4), 16)
    for i in range(0, size - 3, 2):
        v = struct.unpack_from("<I", img, off + i)[0]
        if v in byaddr: taken.add(byaddr[v])

# what a call through a pointer reaches, by who makes it (the sources: the tick's callback, the second core's job,
# the engines' table, the sequencer's hooks, the store's flash steps); any other is listed, its targets unknown
ENG = sorted(f for f in taken if re.search(r"_(render|start|release|block|spread|stop|tick|on|off)$", f))
FAM = {"irq_tick": ["tick"], "cpu1_main": ["job_voices"],
       "seq_play": ["sq_on", "sq_off", "sq_locks"], "play_range": ["sq_on", "sq_off", "sq_locks"],
       "seq_advance_by": ["sq_switched"]}                       # (3.4.2: a pattern changed, its sound before its notes)
# (3.4.3) the project's writer and reader serve the sound from inside (app/project.c project_yield)
for f in ("ev_pack", "save_body", "pwhole"): FAM[f] = ["audio_yield"]
for f in ("ev_unpack", "load_body"): FAM[f] = ["audio_quiet"]   # (a load: the halves given silence, fw/main.c quiet_begin)
for f in ("snd_render_voices", "release_voice", "snd_note_off", "voice_go", "mono_play", "start_note", "lofi_start",
          "tracks_tick"): FAM[f] = ENG
for f in ("store_step", "store_proj_prep", "area_good", "blank4k", "hdr_read", "phdr", "pscan"):
    FAM[f] = [g for g in F if g.startswith("flash_")]
FAM = {k: v for k, v in FAM.items()}
for f in list(F):                                      # (a name the compiler gave a suffix: .constprop.0, .part.0)
    b = f.split(".")[0]
    if b != f and b in FAM and f not in FAM: FAM[f] = FAM[b]
unk = sorted(f for f in F if F[f]["ind"] and f not in FAM)
memo, onstack, recur = {}, set(), set()
def depth(f, ind_ok=True):
    if f not in F: return 0, [f + "?"]
    if f in memo: return memo[f]
    if f in onstack: recur.add(f); return 0, [f + " (recursion)"]
    onstack.add(f)
    best, path = 0, []
    for c in F[f]["calls"]:
        d, p = depth(c)
        if d > best: best, path = d, p
    if F[f]["ind"] and ind_ok:
        for c in FAM.get(f, []):
            if c == f: continue
            d, p = depth(c)
            if d > best: best, path = d, ["*" + p[0]] + p[1:]
    onstack.discard(f)
    memo[f] = (F[f]["frame"] + best, [f"{f}({F[f]['frame']})"] + path)
    return memo[f]

# (the rooms: fw/ram.ld's .stack, 512 bytes for the interrupts (ssp), the rest the main stack; hal/fm1/cpu1_entry.S)
ROOTS = [("app_main", "core 0, main stack", 0x1C00), ("irq_tick", "core 0, the tick's interrupt (+28 its entry)", 0x200),
         ("job_voices", "core 2", 6144)]
bad = 0
for r, what, room in ROOTS:
    d, p = depth(r)
    if r == "irq_tick": d += 28
    flag = "OVER" if d > room else "ok"
    if d > room: bad = 1
    print(f"{r:12s} {d:6d} of {room:5d} bytes  {flag}  [{what}]")
    print("    " + " > ".join(p[:24]) + (" ..." if len(p) > 24 else ""))
if recur: print("recursion:", ", ".join(sorted(recur)))
print("calls through a pointer not followed (their targets unknown):", ", ".join(unk) or "none")
sys.exit(bad)
