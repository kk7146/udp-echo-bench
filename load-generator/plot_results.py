import os
from pathlib import Path

import numpy as np
import pandas as pd
import matplotlib.pyplot as plt


RESULT_DIR = Path("result")
FIG_DIR = Path("figures")


def ensure_dirs():
    FIG_DIR.mkdir(parents=True, exist_ok=True)


def load_data():
    metadata = pd.read_csv(RESULT_DIR / "metadata.csv")
    summary = pd.read_csv(RESULT_DIR / "summary.csv")
    worker_summary = pd.read_csv(RESULT_DIR / "worker_summary.csv")
    packet_rtt = pd.read_csv(RESULT_DIR / "packet_rtt.csv")
    return metadata, summary, worker_summary, packet_rtt


def ns_to_ms(series):
    return series / 1_000_000.0


def save_figure(fig, name):
    png_path = FIG_DIR / f"{name}.png"
    pdf_path = FIG_DIR / f"{name}.pdf"
    fig.savefig(png_path, dpi=300, bbox_inches="tight")
    fig.savefig(pdf_path, bbox_inches="tight")
    print(f"saved: {png_path}")
    print(f"saved: {pdf_path}")


def plot_rtt_cdf(packet_rtt, metadata):
    df = packet_rtt[packet_rtt["lost"] == 0].copy()
    if df.empty:
        print("No successful RTT samples for CDF.")
        return

    df["rtt_ms"] = ns_to_ms(df["rtt_ns"])

    fig, ax = plt.subplots(figsize=(6.5, 4.0))

    # 전체 CDF
    all_rtt = np.sort(df["rtt_ms"].to_numpy())
    all_y = np.arange(1, len(all_rtt) + 1) / len(all_rtt)
    ax.plot(all_rtt, all_y, label="Overall", linewidth=2)

    # worker별 CDF
    for worker_id, group in df.groupby("worker_id"):
        rtt = np.sort(group["rtt_ms"].to_numpy())
        y = np.arange(1, len(rtt) + 1) / len(rtt)
        ax.plot(rtt, y, label=f"Worker {worker_id}", alpha=0.8)

    payload_size = int(metadata.loc[0, "payload_size"])
    workers = int(metadata.loc[0, "workers"])
    pps = int(metadata.loc[0, "pps_per_worker"])

    ax.set_title(f"RTT CDF ({workers} workers, {pps} pps/worker, payload={payload_size}B)")
    ax.set_xlabel("RTT (ms)")
    ax.set_ylabel("CDF")
    ax.grid(True, alpha=0.3)
    ax.legend()
    save_figure(fig, "rtt_cdf")
    plt.close(fig)


def plot_rtt_histogram(packet_rtt, metadata):
    df = packet_rtt[packet_rtt["lost"] == 0].copy()
    if df.empty:
        print("No successful RTT samples for histogram.")
        return

    df["rtt_ms"] = ns_to_ms(df["rtt_ns"])

    fig, ax = plt.subplots(figsize=(6.5, 4.0))

    ax.hist(df["rtt_ms"], bins=80)

    payload_size = int(metadata.loc[0, "payload_size"])
    workers = int(metadata.loc[0, "workers"])
    pps = int(metadata.loc[0, "pps_per_worker"])

    ax.set_title(f"RTT Histogram ({workers} workers, {pps} pps/worker, payload={payload_size}B)")
    ax.set_xlabel("RTT (ms)")
    ax.set_ylabel("Packet count")
    ax.grid(True, alpha=0.3)
    save_figure(fig, "rtt_histogram")
    plt.close(fig)


def plot_rtt_timeseries(packet_rtt, metadata):
    df = packet_rtt.copy()

    # lost 패킷은 NaN 처리해서 시계열에서 빠진 지점 보이게
    df["rtt_ms"] = np.where(df["lost"] == 0, ns_to_ms(df["rtt_ns"]), np.nan)

    fig, ax = plt.subplots(figsize=(8.0, 4.2))

    for worker_id, group in df.groupby("worker_id"):
        group = group.sort_values("seq")
        ax.plot(group["seq"], group["rtt_ms"], label=f"Worker {worker_id}", linewidth=0.8)

    payload_size = int(metadata.loc[0, "payload_size"])
    workers = int(metadata.loc[0, "workers"])
    pps = int(metadata.loc[0, "pps_per_worker"])

    ax.set_title(f"RTT Time Series ({workers} workers, {pps} pps/worker, payload={payload_size}B)")
    ax.set_xlabel("Packet sequence")
    ax.set_ylabel("RTT (ms)")
    ax.grid(True, alpha=0.3)
    ax.legend()
    save_figure(fig, "rtt_timeseries")
    plt.close(fig)


def plot_worker_boxplot(packet_rtt, metadata):
    df = packet_rtt[packet_rtt["lost"] == 0].copy()
    if df.empty:
        print("No successful RTT samples for boxplot.")
        return

    df["rtt_ms"] = ns_to_ms(df["rtt_ns"])

    grouped = []
    labels = []
    for worker_id, group in sorted(df.groupby("worker_id"), key=lambda x: x[0]):
        grouped.append(group["rtt_ms"].to_numpy())
        labels.append(f"W{worker_id}")

    fig, ax = plt.subplots(figsize=(6.0, 4.0))
    ax.boxplot(grouped, labels=labels, showfliers=False)

    payload_size = int(metadata.loc[0, "payload_size"])
    workers = int(metadata.loc[0, "workers"])
    pps = int(metadata.loc[0, "pps_per_worker"])

    ax.set_title(f"Worker-wise RTT Distribution ({workers} workers, {pps} pps/worker, payload={payload_size}B)")
    ax.set_xlabel("Worker")
    ax.set_ylabel("RTT (ms)")
    ax.grid(True, alpha=0.3)
    save_figure(fig, "worker_rtt_boxplot")
    plt.close(fig)


def print_text_summary(summary, worker_summary, metadata):
    print("\n=== Experiment Metadata ===")
    print(metadata.to_string(index=False))

    print("\n=== Overall Summary ===")
    print(summary.to_string(index=False))

    print("\n=== Worker Summary ===")
    print(worker_summary.to_string(index=False))


def main():
    ensure_dirs()
    metadata, summary, worker_summary, packet_rtt = load_data()

    print_text_summary(summary, worker_summary, metadata)

    plot_rtt_cdf(packet_rtt, metadata)
    plot_rtt_histogram(packet_rtt, metadata)
    plot_rtt_timeseries(packet_rtt, metadata)
    plot_worker_boxplot(packet_rtt, metadata)


if __name__ == "__main__":
    main()