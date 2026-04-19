#!/usr/bin/env python3
"""Plot throughput vs. thread count from benchmark CSV output."""

import argparse
import csv
import os
import sys
from datetime import datetime
from collections import defaultdict

import matplotlib.pyplot as plt
import matplotlib.ticker as ticker


def main():
    parser = argparse.ArgumentParser(description="Plot throughput vs. threads from benchmark CSV")
    parser.add_argument("csv", help="Input CSV file")
    parser.add_argument("--save", metavar="FILE",
                        help="Save plot to PNG file (timestamp appended)")
    parser.add_argument("--logx", action="store_true",
                        help="Use logarithmic x axis")
    args = parser.parse_args()

    # data[bench_name][(is_glibc, threads)] = throughput
    data = defaultdict(dict)

    with open(args.csv) as f:
        reader = csv.DictReader(f)
        for row in reader:
            bench = row["benchmark"]
            threads = int(row["n_threads"])
            is_glibc = int(row["is_glibc"])
            throughput = float(row["throughput_alloc_per_s"])
            data[bench][(is_glibc, threads)] = throughput

    if not data:
        print("No data found in CSV.", file=sys.stderr)
        sys.exit(1)

    benchmarks = sorted(data.keys())
    n = len(benchmarks)
    cols = min(n, 3)
    rows = (n + cols - 1) // cols

    fig, axes = plt.subplots(rows, cols, figsize=(6 * cols, 4.5 * rows), squeeze=False)

    for idx, bench in enumerate(benchmarks):
        ax = axes[idx // cols][idx % cols]
        entries = data[bench]

        # Separate reclaim (0) and glibc (1)
        reclaim_pts = sorted((t, v) for (g, t), v in entries.items() if g == 0)
        glibc_pts = sorted((t, v) for (g, t), v in entries.items() if g == 1)

        if reclaim_pts:
            threads_r, tp_r = zip(*reclaim_pts)
            ax.plot(threads_r, tp_r, "o-", label="reclaim", color="#1f77b4")
        if glibc_pts:
            threads_g, tp_g = zip(*glibc_pts)
            ax.plot(threads_g, tp_g, "s--", label="glibc", color="#ff7f0e")

        ax.set_title(bench)
        ax.set_xlabel("Threads")
        ax.set_ylabel("Throughput (alloc/s)")
        if args.logx:
            ax.set_xscale("log", base=2)
            ax.xaxis.set_major_formatter(ticker.FuncFormatter(lambda x, _: f"{int(x)}"))
            ax.xaxis.set_minor_formatter(ticker.NullFormatter())
        ax.legend()
        ax.grid(True, alpha=0.3)

    # Hide unused subplots
    for idx in range(n, rows * cols):
        axes[idx // cols][idx % cols].set_visible(False)

    fig.suptitle("Throughput vs. Thread Count", fontsize=14, fontweight="bold")
    fig.tight_layout()

    if args.save:
        os.makedirs("plots", exist_ok=True)
        ts = datetime.now().strftime("%Y%m%d_%H%M%S")
        filename = f"{args.save}_{ts}.png"
        path = os.path.join("plots", filename)
        fig.savefig(path, dpi=150)
        print(f"Saved to {path}")
    else:
        plt.show()


if __name__ == "__main__":
    main()
