#!/usr/bin/env python3
"""Grid-search custom detector thresholds across every ground-truth image.

The score is the sum over all images of per-type absolute count errors, so a
setting must work on every image at once; tuning to one picture is not
rewarded. Usage: python3 eval/tune.py [--bin ./custom] [--grid small|full]
"""
import argparse
import itertools
import os
import subprocess
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import eval as ev  # noqa: E402

GRIDS = {
    "small": {
        "-vt": [0.2, 0.25, 0.3],
        "-ct": [0.3, 0.35, 0.4],
        "-cos": [0.85, 0.9],
        "-copper": [0.05, 0.06, 0.08],
    },
    "full": {
        "-vt": [0.15, 0.2, 0.25, 0.3, 0.4],
        "-ct": [0.25, 0.3, 0.35, 0.4, 0.5],
        "-cos": [0.8, 0.85, 0.9, 0.95],
        "-copper": [0.04, 0.05, 0.06, 0.08, 0.1],
        "-inner": [0.6, 0.7, 0.8],
    },
}


def run(binary, image, outdir, params):
    cmd = [binary, "-f", image, "-o", outdir]
    for k, v in params.items():
        cmd += [k, str(v)]
    subprocess.run(cmd, check=True, stdout=subprocess.DEVNULL)
    return ev.read_results(os.path.join(outdir, "results.txt"))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--bin", default=os.path.join(ev.ROOT, "custom"))
    ap.add_argument("--grid", choices=GRIDS, default="small")
    ap.add_argument("--images", default=os.path.join(ev.ROOT, "images"))
    args = ap.parse_args()
    truth = ev.read_ground_truth(os.path.join(ev.ROOT, "eval", "ground_truth.txt"))
    grid = GRIDS[args.grid]
    keys = list(grid)
    results = []
    with tempfile.TemporaryDirectory() as tmp:
        for combo in itertools.product(*(grid[k] for k in keys)):
            params = dict(zip(keys, combo))
            score = value_err = 0
            for name, gt in truth.items():
                got = run(args.bin, os.path.join(args.images, name + ".ppm"), tmp, params)
                score += sum(abs(got[t] - gt[t]) for t in ev.TYPES)
                value_err += abs(ev.cents(got) - ev.cents(gt))
            results.append((score, value_err, params))
            print(f"score={score:3d} value_err=${value_err / 100:6.2f}  {params}", flush=True)
    results.sort(key=lambda r: (r[0], r[1]))
    print("\nbest settings (count error, value error):")
    for score, value_err, params in results[:5]:
        print(f"  {score:3d}  ${value_err / 100:6.2f}  {params}")


if __name__ == "__main__":
    main()
