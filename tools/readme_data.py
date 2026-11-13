#!/usr/bin/env python3
"""results/build.json (tools/record_build.py) -> results/derived/readme_data.json with chart rows."""
import json, os
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
b = json.load(open(os.path.join(ROOT, "results/build.json")))
b["loc_rows"] = [{"part": "kernel (C + asm)", "lines": b["loc"]["kernel"]}, {"part": "libc (incl. vendored musl libm)", "lines": b["loc"]["libc"]},
                 {"part": "user programs", "lines": b["loc"]["user"]}]
b["loc_total"] = sum(b["loc"].values())
wad = os.path.getsize(os.path.join(ROOT, "doom/wad/freedoom1.wad")) / 1e6
b["size_rows"] = [{"item": "kernel", "mb": round(b["kernel_kb"] / 1e3, 2)}, {"item": "Freedoom IWAD", "mb": round(wad, 1)},
                  {"item": "initrd (all programs + IWAD)", "mb": b["initrd_mb"]}, {"item": "bootable ISO", "mb": b["iso_mb"]}]
os.makedirs(os.path.join(ROOT, "results/derived"), exist_ok=True)
json.dump(b, open(os.path.join(ROOT, "results/derived/readme_data.json"), "w"), indent=1)
print(json.dumps(b)[:300])
