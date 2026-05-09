#!/usr/bin/env python3
"""
Plot benchmark results for Cache-Adaptive B-Trees
"""

import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
import sys

def plot_algorithm_comparison(csv_file):
    """Plot algorithm comparison from benchmark results"""
    df = pd.read_csv(csv_file)
    
    plt.figure(figsize=(12, 6))
    sns.barplot(data=df, x='workload', y='avg_reward', hue='algorithm')
    plt.title('Algorithm Performance Comparison')
    plt.ylabel('Average Reward')
    plt.xlabel('Workload')
    plt.xticks(rotation=45)
    plt.tight_layout()
    plt.savefig('results/figures/algorithm_comparison.png', dpi=300)
    print("Plot saved to: results/figures/algorithm_comparison.png")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python plot_results.py <results.csv>")
        sys.exit(1)
    
    plot_algorithm_comparison(sys.argv[1])
