import csv
from collections import defaultdict
import math
import matplotlib.pyplot as plt


def load_distribution(path):
    #Returns {allocator name: [(percentile, ticks), ...]}, fraction from 0 to 1
    curves = defaultdict(list)
    with open(path, newline="") as f:
        for row in csv.DictReader(f):
            curves[row["allocator"]].append(
                (float(row["percentile"])/100,
                  int(row["cycles"])
                ))

    return curves

# The cycle counter on this machine ticks at 2.1 GHz
# so 2.1 ticks = 1ns 

TICKS_PER_NS = 2.1 
COLORS = {
    "malloc" : "#d95f02",
    "FixedBlockPool" : "#1b9e77",
    "SlabAllocator" : "#7570b3"
}

STYLES = {
    "malloc" :{"linewidth":4, "linestyle":"-"},
    "FixedBlockPool" : {"linewidth": 2, "linestyle":"--"},
    "SlabAllocator" : {"linewidth":1.2, "linestyle":":"}
}
def plot_distribution(curves, title, out_path):
    fig, ax = plt.subplots(figsize=(9,5))

    for name, points in curves.items():
        xs = []
        ys = []

        for percentile, ticks in points:
            if percentile >= 1.0:
                continue

            xs.append(-math.log10(1.0 - percentile))
            ys.append(ticks / TICKS_PER_NS)

        ax.plot(xs,ys, drawstyle="steps-post", label=name, 
                color=COLORS[name], **STYLES[name])



    ax.set_xticks([1,2,3,4,5,6])
    ax.set_xticklabels(["90%", "99%", "99.9%","99.99%", "99.999%","99.9999%"])
    ax.set_xlabel("Percentile")
    ax.set_ylabel("Latency (ns)")
    ax.set_title(title)
    ax.grid(True, alpha=0.3)
    ax.legend()
    ax.set_yscale("log")
    fig.tight_layout()
    fig.savefig(out_path, dpi=150)
    plt.close(fig)


def main():
    plot_distribution(
        load_distribution("results/latency_distribution.csv"),
        "Best case: allocate and free one block at a time", 
        "results/latency_distribution.png",
    )
    plot_distribution(
        load_distribution("results/latency_distribution_batch.csv"),
        "5000 live blocks, shuffled frees", 
        "results/latency_distribution_batch.png",
    )



if __name__ == "__main__":
    main()
        