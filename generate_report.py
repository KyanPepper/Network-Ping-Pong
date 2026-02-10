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
min_rtt = min(rtt[1:7])  # Skip first (warmup artifact), look at small messages
latency = min_rtt / 2
max_bw = max(bandwidth)

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
        ["Buffer Size", "~1 KB", "Send time increase threshold"],
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

    # Analysis text
    analysis = """
How I Got These Numbers:

Latency: For tiny messages (a few bytes), transfer time is negligible—almost all
the time is overhead. I took the smallest RTT I measured (0.24 μs at 4 bytes) and
divided by 2 to get one-way latency: 0.12 μs.

Bandwidth: At larger sizes, data transfer dominates. Peak bandwidth was 27 GB/s
at 128 KB. This is way too fast for a network—it means the processes were on the
same machine using shared memory.

Buffer Size: MPI buffers small messages so Send() returns immediately. I looked
for where send times jumped (around 1-2 KB), indicating the switch from buffered
to blocking mode.


Communication Model:

    T(n) = α + n/β

Where T(n) is transfer time for n bytes, α is latency, β is bandwidth.
With my estimates: T(n) = 0.12 + n/27000 (μs)


Notes:

- The 120 ns latency and 27 GB/s bandwidth confirm shared-memory communication
- First message (1 byte) was slower due to warmup effects
- Bandwidth peaks at 128 KB then drops slightly for larger messages (cache effects)
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
