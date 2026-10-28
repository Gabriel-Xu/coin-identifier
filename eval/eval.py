#!/usr/bin/env python3
"""Score detector outputs against eval/ground_truth.txt.

Reads out/<impl>/<image>/results.txt for every image listed in the ground
truth and prints per-type absolute count errors, the total-coin error and the
dollar-value error, summed across all images so that no single image dominates.
"""
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TYPES = ["penny", "nickel", "dime", "quarter", "half dollar", "dollar coin"]
PLURALS = {"penny": "pennies", "nickel": "nickels", "dime": "dimes", "quarter": "quarters",
           "half dollar": "half dollars", "dollar coin": "dollar coins"}
CENTS = {"penny": 1, "nickel": 5, "dime": 10, "quarter": 25, "half dollar": 50, "dollar coin": 100}


def read_ground_truth(path):
    truth = {}
    with open(path) as f:
        for line in f:
            line = line.split("#", 1)[0].strip()
            if not line:
                continue
            name, *fields = line.split()
            counts = {t: 0 for t in TYPES}
            for field in fields:
                key, value = field.split("=")
                key = key.replace("_", " ")
                if key not in counts:
                    sys.exit(f"ground truth: unknown coin type '{key}' for {name}")
                counts[key] = int(value)
            truth[name] = counts
    return truth


def read_results(path):
    counts = {t: 0 for t in TYPES}
    with open(path) as f:
        for line in f:
            m = re.match(r"(\d+) (.+)$", line.strip())
            if not m:
                continue
            for t, plural in PLURALS.items():
                if m.group(2) == plural:
                    counts[t] = int(m.group(1))
    return counts


def cents(counts):
    return sum(CENTS[t] * n for t, n in counts.items())


def main():
    truth = read_ground_truth(os.path.join(ROOT, "eval", "ground_truth.txt"))
    impls = [d for d in sorted(os.listdir(os.path.join(ROOT, "out"))) if
             os.path.isdir(os.path.join(ROOT, "out", d))] if os.path.isdir(os.path.join(ROOT, "out")) else []
    if not impls:
        sys.exit("no out/<impl>/ directories; run `make eval` first")
    header = f"{'impl':8} {'image':10} " + " ".join(f"{t[:6]:>6}" for t in TYPES) + "   coins   value   accuracy"
    print(header)
    print("-" * len(header))
    worst = 0
    for impl in impls:
        tot_err = {t: 0 for t in TYPES}
        tot_coins = tot_value = 0
        for name, gt in truth.items():
            path = os.path.join(ROOT, "out", impl, name, "results.txt")
            if not os.path.exists(path):
                print(f"{impl:8} {name:10} (missing {os.path.relpath(path, ROOT)})")
                continue
            got = read_results(path)
            errs = {t: got[t] - gt[t] for t in TYPES}
            dc = sum(got.values()) - sum(gt.values())
            dv = cents(got) - cents(gt)
            print(f"{impl:8} {name:10} " + " ".join(f"{errs[t]:+6d}" for t in TYPES) +
                  f"   {dc:+5d}   {dv / 100:+6.2f}  {1 - abs(dc) / sum(gt.values()):.2%}")
            for t in TYPES:
                tot_err[t] += abs(errs[t])
            tot_coins += abs(dc)
            tot_value += abs(dv)
        print(f"{impl:8} {'|abs sum|':10} " + " ".join(f"{tot_err[t]:6d}" for t in TYPES) +
              f"   {tot_coins:5d}   {tot_value / 100:6.2f}")
        print("-" * len(header))
        worst = max(worst, sum(tot_err.values()))
    print("(columns are detected minus true counts; last row per impl sums absolute errors)")


if __name__ == "__main__":
    main()
