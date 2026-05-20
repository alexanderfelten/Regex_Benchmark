import pandas as pd
import matplotlib.pyplot as plt
import numpy as np

df = pd.read_csv("results.csv")

df = df[df["length"] != "length"]

df["length"] = df["length"].astype(int)
df["throughput_strings_per_sec"] = df["throughput_strings_per_sec"].astype(float)
df["matches"] = df["matches"].astype(int) 

pivot = df.pivot(
    index="length",
    columns="mode",
    values="throughput_strings_per_sec"
)

matches = df.pivot(
    index="length",
    columns="mode",
    values="matches"
)

lengths = pivot.index.astype(str)
x = np.arange(len(lengths))
width = 0.2

plt.figure(figsize=(10, 5))

bars_parallel = plt.bar(
    x - width / 2 - 0.05,
    pivot["parallel"],
    width,
    label="Parallel"
)

bars_single = plt.bar(
    x + width / 2 + 0.05,
    pivot["single"],
    width,
    label="Single-Threaded"
)

plt.xlabel("String length")
plt.ylabel("Throughput (strings/sec)")
plt.title("Regex throughput: single vs parallel")
plt.xticks(x, lengths)
plt.legend()

def annotate(bars, values):
    for bar, val in zip(bars, values):
        height = bar.get_height()
        plt.text(
            bar.get_x() + bar.get_width() / 2 + 0.1,
            height * 1.01,
            f"{int(val)}",
            ha="center",
            va="bottom",
            fontsize=9,
            rotation=45,
        )

#annotate(bars_parallel, matches["parallel"])
#annotate(bars_single, matches["single"])


plt.tight_layout()
plt.show()
