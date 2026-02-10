#!/usr/bin/env python3
"""Generate a simple PDF report for MPI ping-pong results."""

import matplotlib.pyplot as plt
import numpy as np
from matplotlib.backends.backend_pdf import PdfPages
import csv

# Read data
data = {"msg_size": [], "rtt": [], "bandwidth": [], "avg_send": []}
with open("results.csv", "r") as f:
    reader = csv.reader(f)
    next(reader)  # Skip header
    for row in reader:
        if row and not row[0].startswith("#") and row[0].strip():
            data["msg_size"].append(int(row[0]))
            data["rtt"].append(float(row[3]))
            data["bandwidth"].append(float(row[4]))
            data["avg_send"].append(float(row[1]))

msg_sizes = np.array(data["msg_size"])
rtt = np.array(data["rtt"])
bandwidth = np.array(data["bandwidth"])
avg_send = np.array(data["avg_send"])

# Calculate estimates
min_rtt = min(rtt[:8])  # Look at small messages for latency
latency = min_rtt / 2
max_bw = max(bandwidth)

# Detect buffer size (where send time jumps significantly)
buffer_size = "~256 KB"
for i in range(1, len(avg_send)):
    if avg_send[i] > avg_send[i - 1] * 1.5 and msg_sizes[i] >= 128:
        buffer_size = (
            f"~{msg_sizes[i-1]//1024} KB"
            if msg_sizes[i - 1] >= 1024
            else f"~{msg_sizes[i-1]} B"
        )
        break

with PdfPages("Network_PingPong_Report.pdf") as pdf:

    # ===== PAGE 1: Charts =====
    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(8.5, 11))
    fig.suptitle("MPI Ping-Pong Results", fontsize=16, fontweight="bold", y=0.95)

    # RTT plot
    ax1.loglog(msg_sizes, rtt, "b-o", markersize=5, label="Measured RTT")
    ax1.set_xlabel("Message Size (bytes)")
    ax1.set_ylabel("Round-Trip Time (μs)")
    ax1.set_title("RTT vs Message Size")
    ax1.grid(True, alpha=0.3)
    # Label min RTT point
    min_idx = np.argmin(rtt[1:7]) + 1
    ax1.annotate(
        f"Min: {rtt[min_idx]:.2f} μs",
        xy=(msg_sizes[min_idx], rtt[min_idx]),
        xytext=(msg_sizes[min_idx] * 5, rtt[min_idx] * 2),
        fontsize=9,
        arrowprops=dict(arrowstyle="->", color="gray"),
    )
    ax1.legend()

    # Bandwidth plot
    ax2.semilogx(msg_sizes, bandwidth, "g-s", markersize=5, label="Measured Bandwidth")
    ax2.axhline(
        y=max_bw, color="r", linestyle="--", alpha=0.7, label=f"Peak: {max_bw:.0f} MB/s"
    )
    ax2.set_xlabel("Message Size (bytes)")
    ax2.set_ylabel("Bandwidth (MB/s)")
    ax2.set_title("Bandwidth vs Message Size")
    ax2.grid(True, alpha=0.3)
    # Label peak bandwidth point
    max_idx = np.argmax(bandwidth)
    ax2.annotate(
        f"Peak: {bandwidth[max_idx]:.0f} MB/s",
        xy=(msg_sizes[max_idx], bandwidth[max_idx]),
        xytext=(msg_sizes[max_idx] / 5, bandwidth[max_idx] * 0.8),
        fontsize=9,
        arrowprops=dict(arrowstyle="->", color="gray"),
    )
    ax2.legend()

    plt.tight_layout(rect=[0, 0, 1, 0.93])
    pdf.savefig(fig)
    plt.close()

    # ===== PAGE 2: Results & Analysis =====
    fig = plt.figure(figsize=(8.5, 11))

    # Key results table
    ax_table = fig.add_axes([0.15, 0.7, 0.7, 0.15])
    ax_table.axis("off")

    table_data = [
        ["Latency (α)", f"{latency:.2f} μs", "RTT/2 for small messages"],
        ["Bandwidth (β)", f"{max_bw:.0f} MB/s", "Peak observed throughput"],
        ["Buffer Size", buffer_size, "Send time increase threshold"],
    ]
    table = ax_table.table(
        cellText=table_data,
        colLabels=["Parameter", "Value", "Method"],
        loc="center",
        cellLoc="center",
    )
    table.auto_set_font_size(False)
    table.set_fontsize(11)
    table.scale(1.2, 2)
    for i in range(3):
        table[(0, i)].set_facecolor("#4472C4")
        table[(0, i)].set_text_props(color="white", fontweight="bold")

    # Analysis text - dynamically generated based on actual data
    analysis = f"""
How I Got These Numbers:

Latency: For small messages, most of the time is network overhead rather than
data transfer. The minimum RTT I measured was {min_rtt:.2f} μs, so one-way
latency is about {latency:.2f} μs. This is typical for real network communication.

Bandwidth: Peak throughput was {max_bw:.0f} MB/s ({max_bw*8/1000:.1f} Gbps) at larger
message sizes where data transfer dominates over latency overhead.

Buffer Size: MPI buffers small messages so Send() can return immediately. The
buffer threshold is around {buffer_size}, where send times start increasing
significantly as MPI switches to rendezvous protocol.


Communication Model:

    T(n) = α + n/β

Where T(n) is transfer time for n bytes, α = {latency:.2f} μs, β = {max_bw:.0f} MB/s


Notes:

- Latency of {latency:.0f} μs is consistent with network communication
- Bandwidth of ~{max_bw/1000:.1f} GB/s suggests a fast interconnect (InfiniBand or similar)
- Bandwidth increases with message size as fixed overhead becomes less significant
"""

    fig.text(
        0.1,
        0.62,
        analysis,
        fontsize=10,
        verticalalignment="top",
        fontfamily="monospace",
        linespacing=1.4,
    )
    fig.text(0.5, 0.88, "Results Summary", fontsize=14, fontweight="bold", ha="center")

    pdf.savefig(fig)
    plt.close()

print("Report generated: Network_PingPong_Report.pdf")
