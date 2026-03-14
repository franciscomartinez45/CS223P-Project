#!/usr/bin/env python3

import sys
import os
import glob
import numpy as np
import pandas as pd
import matplotlib as mpl
import matplotlib.pyplot
import matplotlib.ticker

PROTOCOL_STYLES = {
    "OCC": dict(color="#e05c2e", marker="o", linestyle="-"),
    "2PL": dict(color="#2e7ae0", marker="s", linestyle="--"),
}

BAR_COLORS = {
    "OCC": "#e05c2e",
    "2PL": "#2e7ae0",
}


def find_csvs(folder):
    pattern = os.path.join(folder, "**", "*.csv")
    return sorted(glob.glob(pattern, recursive=True))


def _save(fig, path):
    fig.tight_layout()
    fig.savefig(path, dpi=150)
    mpl.pyplot.close(fig)


def split_sweeps(df):
    dominant_hot = df.groupby("hot_prob")["hot_prob"].count().idxmax()
    thread_df = df[df["hot_prob"] == dominant_hot].sort_values("threads")

    dominant_threads = df.groupby("threads")["threads"].count().idxmax()
    contention_df = (df[df["threads"] == dominant_threads]
                     .groupby(["protocol", "hot_prob"], as_index=False).first()
                     .sort_values("hot_prob"))

    return thread_df, contention_df


def plot_throughput_threads(thread_df, out_dir, stem, workload):
    fig, ax = mpl.pyplot.subplots(figsize=(6, 4))
    for proto, grp in thread_df.groupby("protocol"):
        ax.plot(grp["threads"], grp["throughput_tps"], label=proto,
                linewidth=2, markersize=7, **PROTOCOL_STYLES.get(proto, {}))
    ax.set_xlabel("Number of Threads")
    ax.set_ylabel("Throughput (TPS)")
    ax.set_title(f"{workload} — Throughput vs Threads")
    ax.legend()
    ax.grid(True, linestyle=":", alpha=0.6)
    ax.xaxis.set_major_locator(mpl.ticker.MaxNLocator(integer=True))
    _save(fig, os.path.join(out_dir, f"{stem}_throughput_threads.png"))


def plot_throughput_contention(contention_df, out_dir, stem, workload):
    fig, ax = mpl.pyplot.subplots(figsize=(6, 4))
    for proto, grp in contention_df.groupby("protocol"):
        ax.plot(grp["hot_prob"], grp["throughput_tps"], label=proto,
                linewidth=2, markersize=7, **PROTOCOL_STYLES.get(proto, {}))
    ax.set_xlabel("Hot-Record Probability (contention)")
    ax.set_ylabel("Throughput (TPS)")
    ax.set_title(f"{workload} — Throughput vs Contention")
    ax.legend()
    ax.grid(True, linestyle=":", alpha=0.6)
    _save(fig, os.path.join(out_dir, f"{stem}_throughput_contention.png"))


def plot_retry_rate_contention(contention_df, out_dir, stem, workload):
    fig, ax = mpl.pyplot.subplots(figsize=(6, 4))
    for proto, grp in contention_df.groupby("protocol"):
        ax.plot(grp["hot_prob"], grp["retry_rate_pct"], label=proto,
                linewidth=2, markersize=7, **PROTOCOL_STYLES.get(proto, {}))
    ax.set_xlabel("Hot-Record Probability (contention)")
    ax.set_ylabel("Retry Rate (%)")
    ax.set_title(f"{workload} — Retry Rate vs Contention")
    ax.legend()
    ax.grid(True, linestyle=":", alpha=0.6)
    _save(fig, os.path.join(out_dir, f"{stem}_retry_rate_contention.png"))


def plot_resptime_threads(thread_df, out_dir, stem, workload):
    fig, ax = mpl.pyplot.subplots(figsize=(6, 4))
    for proto, grp in thread_df.groupby("protocol"):
        ax.plot(grp["threads"], grp["avg_resp_ms"], label=proto,
                linewidth=2, markersize=7, **PROTOCOL_STYLES.get(proto, {}))
    ax.set_xlabel("Number of Threads")
    ax.set_ylabel("Avg Response Time (ms)")
    ax.set_title(f"{workload} — Avg Response Time vs Threads")
    ax.legend()
    ax.grid(True, linestyle=":", alpha=0.6)
    ax.xaxis.set_major_locator(mpl.ticker.MaxNLocator(integer=True))
    _save(fig, os.path.join(out_dir, f"{stem}_resptime_threads.png"))


def plot_resptime_contention(contention_df, out_dir, stem, workload):
    fig, ax = mpl.pyplot.subplots(figsize=(6, 4))
    for proto, grp in contention_df.groupby("protocol"):
        ax.plot(grp["hot_prob"], grp["avg_resp_ms"], label=proto,
                linewidth=2, markersize=7, **PROTOCOL_STYLES.get(proto, {}))
    ax.set_xlabel("Hot-Record Probability (contention)")
    ax.set_ylabel("Avg Response Time (ms)")
    ax.set_title(f"{workload} — Avg Response Time vs Contention")
    ax.legend()
    ax.grid(True, linestyle=":", alpha=0.6)
    _save(fig, os.path.join(out_dir, f"{stem}_resptime_contention.png"))


def plot_resptime_distribution(df, out_dir, stem, workload):
    thread_df = df.sort_values("threads")
    protocols = sorted(thread_df["protocol"].unique())
    percentiles = ["p50_resp_ms", "p95_resp_ms", "max_resp_ms"]
    pct_labels  = ["p50", "p95", "max"]

    threads = sorted(thread_df["threads"].unique())
    dominant_hot = thread_df.groupby("hot_prob")["hot_prob"].count().idxmax()
    plot_df = (thread_df[thread_df["hot_prob"] == dominant_hot]
               .groupby(["protocol", "threads"], as_index=False).first())

    n_threads = len(threads)
    n_pct     = len(percentiles)
    n_proto   = len(protocols)
    x = np.arange(n_threads)
    group_width = 0.8
    bar_width   = group_width / (n_proto * n_pct)

    fig, ax = mpl.pyplot.subplots(figsize=(max(8, 2 * n_threads), 5))
    for pi, proto in enumerate(protocols):
        pdata = plot_df[plot_df["protocol"] == proto].set_index("threads")
        for qi, (col, label) in enumerate(zip(percentiles, pct_labels)):
            offsets = (pi * n_pct + qi - (n_proto * n_pct) / 2 + 0.5) * bar_width
            vals = [pdata.loc[t, col] if t in pdata.index else 0 for t in threads]
            ax.bar(x + offsets, vals, bar_width * 0.9,
                   label=f"{proto} {label}",
                   color=BAR_COLORS.get(proto, "grey"),
                   alpha=0.5 + 0.25 * qi)

    ax.set_xlabel("Number of Threads")
    ax.set_ylabel("Response Time (ms)")
    ax.set_title(f"{workload} — Response Time Distribution (p50 / p95 / max)")
    ax.set_xticks(x)
    ax.set_xticklabels([str(t) for t in threads])
    ax.legend(ncol=n_proto, fontsize=8)
    ax.grid(True, axis="y", linestyle=":", alpha=0.6)
    _save(fig, os.path.join(out_dir, f"{stem}_resptime_distribution.png"))


def process_csv(csv_path):
    df = pd.read_csv(csv_path)
    out_dir = os.path.join(os.path.dirname(csv_path), "plots")
    os.makedirs(out_dir, exist_ok=True)
    stem     = os.path.splitext(os.path.basename(csv_path))[0]
    workload = df["workload"].iloc[0]

    thread_df, contention_df = split_sweeps(df)

    if thread_df["threads"].nunique() > 1:
        plot_throughput_threads(thread_df, out_dir, stem, workload)
        plot_resptime_threads(thread_df, out_dir, stem, workload)
        plot_resptime_distribution(df, out_dir, stem, workload)

    if contention_df["hot_prob"].nunique() > 1:
        plot_throughput_contention(contention_df, out_dir, stem, workload)
        plot_retry_rate_contention(contention_df, out_dir, stem, workload)
        plot_resptime_contention(contention_df, out_dir, stem, workload)


def main():
    folder = sys.argv[1]
    csvs = find_csvs(folder)
    for csv_path in csvs:
        process_csv(csv_path)

if __name__ == "__main__":
    main()
