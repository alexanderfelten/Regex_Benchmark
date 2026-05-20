import pandas as pd
import matplotlib.pyplot as plt
df = pd.read_csv('results.csv')

x_labels = df["length"].astype(str)
y =  df["throughput_strings_per_sec"]
matches = df["matches"]

plt.bar(x_labels, y, width= 0.6)
plt.xlabel("String length")
plt.ylabel("Throughput")
plt.title("Regex throughput for variable string lengths")

for i, m in enumerate(matches):
    plt.text(i, y[i] *  1.01, str(m), ha='center', va='bottom', fontsize=10)

plt.tight_layout()
plt.show()