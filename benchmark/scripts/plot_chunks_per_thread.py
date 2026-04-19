#!/usr/bin/env python3
"""Plot throughput vs. chunks per thread from benchmark CSV output."""

import argparse
import csv
import os
import sys
from datetime import datetime
from collections import defaultdict

import matplotlib.pyplot as plt
import matplotlib.ticker as ticker


def find_column(fieldnames, prefix):
    """Find the first column name starting with prefix."""
    for name in fieldnames:
        if name.startswith(prefix):
            return name
    return None


def main():
    parser = argparse.ArgumentParser(description="Plot throughput vs. chunks per thread from benchmark CSV")
    parser.add_argument("csv", help="Input CSV file")
    parser.add_argument("--save", metavar="FILE",
                        help="Save plot to PNG file (timestamp appended)")
    parser.add_argument("--logx", action="store_true",
                        help="Use logarithmic x axis")
    args = parser.parse_args()

    # data[bench_name][(is_glibc, chunks_per_thread)] = throughput
    data = defaultdict(dict)
    col_name = None

    with open(args.csv) as f:
        reader = csv.DictReader(f)
        col_name = find_column(reader.fieldnames, "chunks_per_thread")
        if not col_name:
            print("No column starting with 'chunks_per_thread' found in CSV.", file=sys.stderr)
            sys.exit(1)
        for row in reader:
            bench = row["benchmark"]
            cpt = int(row[col_name])
            is_glibc = int(row["is_glibc"])
            throughput = float(row["throughput_alloc_per_s"])
            data[bench][(is_glibc, cpt)] = throughput

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

        reclaim_pts = sorted((t, v) for (g, t), v in entries.items() if g == 0)
        glibc_pts = sorted((t, v) for (g, t), v in entries.items() if g == 1)

        if reclaim_pts:
            xs, ys = zip(*reclaim_pts)
            ax.plot(xs, ys, "o-", label="reclaim", color="#1f77b4")
        if glibc_pts:
            xs, ys = zip(*glibc_pts)
            ax.plot(xs, ys, "s--", label="glibc", color="#ff7f0e")

        ax.set_title(bench)
        ax.set_xlabel("Chunks per Thread")
        ax.set_ylabel("Throughput (alloc/s)")
        if args.logx:
            ax.set_xscale("log", base=2)
            ax.xaxis.set_major_formatter(ticker.FuncFormatter(lambda x, _: f"{int(x)}"))
            ax.xaxis.set_minor_formatter(ticker.NullFormatter())
        ax.legend()
        ax.grid(True, alpha=0.3)

    for idx in range(n, rows * cols):
        axes[idx // cols][idx % cols].set_visible(False)

    fig.suptitle("Throughput vs. Chunks per Thread", fontsize=14, fontweight="bold")
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
