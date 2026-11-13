#!/usr/bin/env python3
"""Build the ISO with ./iso.sh and record what came out in results/build.json (source of README/video numbers).
Needs the i686-elf cross toolchain on PATH (see README Quickstart)."""
import json, os, subprocess, sys, time
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
env = dict(os.environ)
env["PATH"] = os.path.expanduser("~/opt/cross/bin") + os.pathsep + env["PATH"]
t = time.time()
subprocess.run(["./iso.sh"], cwd=ROOT, env=env, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
secs = time.time() - t
def loc(paths, exts):
    n = 0
    for p in paths:
        for d, dirs, fs in os.walk(os.path.join(ROOT, p)):
            for f in fs:
                if f.endswith(exts):
                    n += len(open(os.path.join(d, f), errors="ignore").read().splitlines())
    return n
bins = sorted(os.listdir(os.path.join(ROOT, "initrd/bin")))
tests = [b for b in bins if b.endswith("test") or b in ("crash", "bigtest", "fbtest")]
res = {"iso_mb": round(os.path.getsize(os.path.join(ROOT, "bobliu.iso")) / 1e6, 1),
       "initrd_mb": round(os.path.getsize(os.path.join(ROOT, "isodir/boot/initrd.tar")) / 1e6, 1),
       "kernel_kb": round(os.path.getsize(os.path.join(ROOT, "isodir/boot/myos.kernel")) / 1e3),
       "programs": len(bins), "test_programs": len(tests), "build_seconds": round(secs),
       "loc": {"kernel": loc(["kernel"], (".c", ".h", ".S")), "libc": loc(["libc"], (".c", ".h", ".S")),
               "user": loc(["user"], (".c", ".h"))}}
os.makedirs(os.path.join(ROOT, "results"), exist_ok=True)
json.dump(res, open(os.path.join(ROOT, "results/build.json"), "w"), indent=1)
print(json.dumps(res))
