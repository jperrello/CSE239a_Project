#!/usr/bin/env python3
# performance_visualizer.py
# Visualize the performance metrics from the NDN Router benchmark tests

import os
import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
import sys
from matplotlib.ticker import ScalarFormatter

def analyze_operations_benchmark(csv_file="operations_benchmark.csv"):
    """Analyze and visualize the operations benchmark results"""
    print(f"Analyzing operations benchmark data from {csv_file}")
    
    if not os.path.exists(csv_file):
        print(f"Error: File {csv_file} not found!")
        return
    
    # Load the data
    df = pd.read_csv(csv_file)
    
    # Check if the data has error records
    error_rows = df[df['ThroughputOpsPerSec'].astype(str).str.contains('ERROR')]
    if not error_rows.empty:
        print("Warning: The following operations had errors:")
        for _, row in error_rows.iterrows():
            print(f"  Operations: {row['OperationCount']} - Error: {row['ThroughputOpsPerSec']}")
        
        # Filter out error rows for visualization
        df = df[~df['ThroughputOpsPerSec'].astype(str).str.contains('ERROR')]
    
    if df.empty:
        print("No valid data to visualize!")
        return
    
    # Create the visualization directory
    os.makedirs("visualizations", exist_ok=True)
    
    # Create a figure with multiple subplots
    fig, axes = plt.subplots(2, 2, figsize=(15, 12))
    fig.suptitle('NDN Router Operations Benchmark Results', fontsize=16)
    
    # Plot 1: Throughput vs Operation Count
    ax1 = axes[0, 0]
    ax1.plot(df['OperationCount'], df['ThroughputOpsPerSec'], 'bo-', linewidth=2, markersize=8)
    ax1.set_xlabel('Number of Operations')
    ax1.set_ylabel('Throughput (ops/sec)')
    ax1.set_title('Throughput vs. Operation Count')
    ax1.grid(True)
    
    # Plot 2: Latency vs Operation Count
    ax2 = axes[0, 1]
    ax2.plot(df['OperationCount'], df['InterestLatencyMean'], 'ro-', label='Interest Latency', linewidth=2)
    ax2.plot(df['OperationCount'], df['DataLatencyMean'], 'go-', label='Data Latency', linewidth=2)
    ax2.plot(df['OperationCount'], df['RetrievalLatencyMean'], 'bo-', label='Retrieval Latency', linewidth=2)
    ax2.set_xlabel('Number of Operations')
    ax2.set_ylabel('Latency (μs)')
    ax2.set_title('Operation Latency vs. Operation Count')
    ax2.legend()
    ax2.grid(True)
    
    # Plot 3: Stash Size vs Operation Count
    ax3 = axes[1, 0]
    ax3.plot(df['OperationCount'], df['MaxStashSize'], 'mo-', linewidth=2, markersize=8)
    ax3.set_xlabel('Number of Operations')
    ax3.set_ylabel('Maximum Stash Size')
    ax3.set_title('Maximum Stash Size vs. Operation Count')
    ax3.grid(True)
    
    # Plot 4: Total Time vs Operation Count
    ax4 = axes[1, 1]
    ax4.plot(df['OperationCount'], df['TotalTimeSeconds'], 'co-', linewidth=2, markersize=8)
    ax4.set_xlabel('Number of Operations')
    ax4.set_ylabel('Total Time (seconds)')
    ax4.set_title('Total Execution Time vs. Operation Count')
    ax4.grid(True)
    
    # Adjust layout and save the figure
    plt.tight_layout(rect=[0, 0, 1, 0.95])
    plt.savefig('visualizations/operations_benchmark.png', dpi=300)
    print("Saved operations benchmark visualization to visualizations/operations_benchmark.png")
    plt.close()
    
    # Create a stash utilization analysis chart if we have detailed data
    analyze_stash_utilization(df['OperationCount'].tolist())

def analyze_baseline_comparison(csv_file="baseline_comparison.csv"):
    """Analyze and visualize the baseline comparison results"""
    print(f"Analyzing baseline comparison data from {csv_file}")
    
    if not os.path.exists(csv_file):
        print(f"Error: File {csv_file} not found!")
        return
    
    # Load the data
    df = pd.read_csv(csv_file)
    
    # Check if the data has error records
    error_rows = df[df['BaselineThroughput'].astype(str).str.contains('ERROR')]
    if not error_rows.empty:
        print("Warning: The following operations had errors:")
        for _, row in error_rows.iterrows():
            print(f"  Operations: {row['OperationCount']} - Error: {row['BaselineThroughput']}")
        
        # Filter out error rows for visualization
        df = df[~df['BaselineThroughput'].astype(str).str.contains('ERROR')]
    
    if df.empty:
        print("No valid data to visualize!")
        return
    
    # Create the visualization directory
    os.makedirs("visualizations", exist_ok=True)
    
    # Create a figure with multiple subplots
    fig, axes = plt.subplots(2, 2, figsize=(15, 12))
    fig.suptitle('NDN Router: Privacy-Preserving vs. Baseline Comparison', fontsize=16)
    
    # Plot 1: Throughput Comparison
    ax1 = axes[0, 0]
    x = np.arange(len(df['OperationCount']))
    width = 0.35
    
    ax1.bar(x - width/2, df['BaselineThroughput'], width, label='Baseline', color='blue', alpha=0.7)
    ax1.bar(x + width/2, df['PrivacyThroughput'], width, label='Privacy-Preserving', color='green', alpha=0.7)
    
    ax1.set_xlabel('Number of Operations')
    ax1.set_ylabel('Throughput (ops/sec)')
    ax1.set_title('Throughput Comparison')
    ax1.set_xticks(x)
    ax1.set_xticklabels(df['OperationCount'])
    ax1.legend()
    ax1.grid(True, axis='y')
    
    # Add overhead text labels
    for i, overhead in enumerate(df['ThroughputOverhead']):
        ax1.text(i, df['BaselineThroughput'].iloc[i] + 5, f"{overhead:.1f}x", 
                 ha='center', va='bottom', fontweight='bold')
    
    # Plot 2: Latency Comparison (Interest)
    ax2 = axes[0, 1]
    
    ax2.bar(x - width/2, df['BaselineInterestLatency'], width, label='Baseline', color='blue', alpha=0.7)
    ax2.bar(x + width/2, df['PrivacyInterestLatency'], width, label='Privacy-Preserving', color='green', alpha=0.7)
    
    ax2.set_xlabel('Number of Operations')
    ax2.set_ylabel('Interest Latency (μs)')
    ax2.set_title('Interest Operation Latency Comparison')
    ax2.set_xticks(x)
    ax2.set_xticklabels(df['OperationCount'])
    ax2.legend()
    ax2.grid(True, axis='y')
    
    # Add overhead text labels
    for i, overhead in enumerate(df['InterestLatencyOverhead']):
        ax2.text(i, df['PrivacyInterestLatency'].iloc[i] + 5, f"{overhead:.1f}x", 
                 ha='center', va='bottom', fontweight='bold')
    
    # Plot 3: Memory Usage Comparison
    ax3 = axes[1, 0]
    
    ax3.bar(x - width/2, df['BaselineMemoryMB'], width, label='Baseline', color='blue', alpha=0.7)
    ax3.bar(x + width/2, df['PrivacyMemoryMB'], width, label='Privacy-Preserving', color='green', alpha=0.7)
    
    ax3.set_xlabel('Number of Operations')
    ax3.set_ylabel('Memory Usage (MB)')
    ax3.set_title('Memory Usage Comparison')
    ax3.set_xticks(x)
    ax3.set_xticklabels(df['OperationCount'])
    ax3.legend()
    ax3.grid(True, axis='y')
    
    # Add overhead text labels
    for i, overhead in enumerate(df['MemoryOverhead']):
        ax3.text(i, df['PrivacyMemoryMB'].iloc[i] + 5, f"{overhead:.1f}x", 
                 ha='center', va='bottom', fontweight='bold')
    
    # Plot 4: Overhead Summary
    ax4 = axes[1, 1]
    
    metrics = ['Throughput', 'Interest Latency', 'Data Latency', 'Retrieval Latency', 'Memory']
    overheads = [df['ThroughputOverhead'].mean(), 
                 df['InterestLatencyOverhead'].mean(),
                 df['DataLatencyOverhead'].mean(), 
                 df['RetrievalLatencyOverhead'].mean(),
                 df['MemoryOverhead'].mean()]
    
    ax4.bar(metrics, overheads, color='purple', alpha=0.7)
    ax4.set_ylabel('Average Overhead Factor (x)')
    ax4.set_title('Privacy-Preserving Overhead Summary')
    ax4.set_ylim(bottom=0)
    plt.setp(ax4.get_xticklabels(), rotation=30, ha='right')
    ax4.grid(True, axis='y')
    
    # Add text labels
    for i, overhead in enumerate(overheads):
        ax4.text(i, overhead + 0.1, f"{overhead:.1f}x", ha='center', va='bottom')
    
    # Adjust layout and save the figure
    plt.tight_layout(rect=[0, 0, 1, 0.95])
    plt.savefig('visualizations/baseline_comparison.png', dpi=300)
    print("Saved baseline comparison visualization to visualizations/baseline_comparison.png")
    plt.close()

def analyze_configuration_benchmark(csv_file="config_benchmark_results.csv"):
    """Analyze and visualize the configuration benchmark results"""
    print(f"Analyzing configuration benchmark data from {csv_file}")
    
    if not os.path.exists(csv_file):
        print(f"Error: File {csv_file} not found!")
        return
    
    # Load the data
    df = pd.read_csv(csv_file)
    
    # Check if the data has error records
    error_rows = df[df['Throughput'].astype(str).str.contains('ERROR')]
    if not error_rows.empty:
        print("Warning: The following configurations had errors:")
        for _, row in error_rows.iterrows():
            config = f"Tree(h={row['TreeHeight']},b={row['BucketCapacity']},s={row['StashLimit']})"
            print(f"  Configuration: {config} - Error: {row['Throughput']}")
        
        # Filter out error rows for visualization
        df = df[~df['Throughput'].astype(str).str.contains('ERROR')]
    
    if df.empty:
        print("No valid data to visualize!")
        return
    
    # Create the visualization directory
    os.makedirs("visualizations", exist_ok=True)
    
    # Create configuration labels
    df['ConfigLabel'] = df.apply(lambda row: f"T{row['TreeHeight']}-B{row['BucketCapacity']}-S{row['StashLimit']}", axis=1)
    
    # Group by tree height
    height_groups = df.groupby('TreeHeight')
    
    # Create a plot for tree height comparison
    plt.figure(figsize=(12, 8))
    plt.title('Impact of Tree Height on Performance', fontsize=16)
    
    for height, group in height_groups:
        if len(group) > 0:  # Only plot groups with data
            plt.plot(group['BucketCapacity'], group['Throughput'], 'o-', 
                     label=f'Tree Height = {height}', linewidth=2, markersize=8)
    
    plt.xlabel('Bucket Capacity')
    plt.ylabel('Throughput (ops/sec)')
    plt.grid(True)
    plt.legend()
    plt.savefig('visualizations/tree_height_impact.png', dpi=300)
    print("Saved tree height impact visualization to visualizations/tree_height_impact.png")
    plt.close()
    
    # Group by bucket capacity
    bucket_groups = df.groupby('BucketCapacity')
    
    # Create a plot for bucket capacity comparison
    plt.figure(figsize=(12, 8))
    plt.title('Impact of Bucket Capacity on Performance', fontsize=16)
    
    for capacity, group in bucket_groups:
        if len(group) > 0:  # Only plot groups with data
            plt.plot(group['TreeHeight'], group['Throughput'], 'o-', 
                     label=f'Bucket Capacity = {capacity}', linewidth=2, markersize=8)
    
    plt.xlabel('Tree Height')
    plt.ylabel('Throughput (ops/sec)')
    plt.grid(True)
    plt.legend()
    plt.savefig('visualizations/bucket_capacity_impact.png', dpi=300)
    print("Saved bucket capacity impact visualization to visualizations/bucket_capacity_impact.png")
    plt.close()
    
    # Create a heatmap for max stash size
    plt.figure(figsize=(10, 8))
    plt.title('Maximum Stash Size by Configuration', fontsize=16)
    
    # Filter for specific configurations to create a cleaner heatmap
    pivot_df = df.pivot_table(index='TreeHeight', columns='BucketCapacity', values='MaxStashSize')
    
    # Plot heatmap
    im = plt.imshow(pivot_df, cmap='YlOrRd')
    plt.colorbar(im, label='Max Stash Size')
    
    # Set labels
    plt.xlabel('Bucket Capacity')
    plt.ylabel('Tree Height')
    
    # Set tick labels
    plt.xticks(range(len(pivot_df.columns)), pivot_df.columns)
    plt.yticks(range(len(pivot_df.index)), pivot_df.index)
    
    # Annotate cells with values
    for i in range(len(pivot_df.index)):
        for j in range(len(pivot_df.columns)):
            if not np.isnan(pivot_df.iloc[i, j]):
                plt.text(j, i, int(pivot_df.iloc[i, j]), 
                         ha="center", va="center", color="black" if pivot_df.iloc[i, j] < 100 else "white")
    
    plt.tight_layout()
    plt.savefig('visualizations/stash_size_heatmap.png', dpi=300)
    print("Saved stash size heatmap to visualizations/stash_size_heatmap.png")
    plt.close()

def analyze_stash_utilization(op_counts):
    """Analyze stash utilization over time from detailed metrics files"""
    
    plt.figure(figsize=(12, 8))
    plt.title('Stash Size History During Operations', fontsize=16)
    
    for ops in op_counts:
        filename = f"operations_{ops}.csv"
        if not os.path.exists(filename):
            print(f"Warning: Detailed metrics file {filename} not found, skipping.")
            continue
        
        # Read the stash history section
        stash_history = []
        with open(filename, 'r') as f:
            in_stash_section = False
            for line in f:
                line = line.strip()
                if line == "Stash Size History":
                    in_stash_section = True
                    continue
                
                if in_stash_section and line and not line.startswith("Metric"):
                    try:
                        stash_history.append(int(line))
                    except ValueError:
                        # Skip non-integer lines
                        pass
        
        if stash_history:
            # Plot stash size over time
            plt.plot(range(len(stash_history)), stash_history, label=f'{ops} Operations')
    
    plt.xlabel('Operation Count')
    plt.ylabel('Stash Size')
    plt.grid(True)
    plt.legend()
    
    plt.tight_layout()
    plt.savefig('visualizations/stash_utilization.png', dpi=300)
    print("Saved stash utilization visualization to visualizations/stash_utilization.png")
    plt.close()

if __name__ == "__main__":
    print("NDN Router Performance Visualizer")
    
    # Create visualizations directory
    os.makedirs("visualizations", exist_ok=True)
    
    if len(sys.argv) > 1:
        # Process specific visualization based on argument
        if sys.argv[1] == "operations":
            analyze_operations_benchmark()
        elif sys.argv[1] == "baseline":
            analyze_baseline_comparison()
        elif sys.argv[1] == "config":
            analyze_configuration_benchmark()
        else:
            print(f"Unknown visualization type: {sys.argv[1]}")
            print("Available types: operations, baseline, config")
    else:
        # Process all visualizations
        print("Generating all visualizations...")
        analyze_operations_benchmark()
        analyze_baseline_comparison()
        analyze_configuration_benchmark()
        print("Visualization complete!")