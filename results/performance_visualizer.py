#!/usr/bin/env python3
# performance_visualizer.py
# Visualize the performance metrics from the NDN Router benchmark tests

import os
import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
import sys
from matplotlib.ticker import ScalarFormatter

def create_enhanced_throughput_visualizations(df):
    """Create enhanced visualizations for throughput data"""
    import matplotlib.pyplot as plt
    import numpy as np
    from matplotlib.ticker import ScalarFormatter
    import seaborn as sns
    
    # Create visualizations directory if it doesn't exist
    os.makedirs("visualizations", exist_ok=True)
    
    # 1. Normalized Throughput Plot - Shows relative performance
    plt.figure(figsize=(10, 6))
    # Normalize to the maximum throughput = 100%
    max_throughput = df['ThroughputOpsPerSec'].max()
    normalized = (df['ThroughputOpsPerSec'] / max_throughput) * 100
    
    # Plot both actual and normalized values
    fig, ax1 = plt.subplots(figsize=(12, 7))
    
    # Plot actual throughput (bars)
    bars = ax1.bar(df['OperationCount'].astype(str), df['ThroughputOpsPerSec'], 
            alpha=0.6, color='steelblue')
    ax1.set_xlabel('Number of Operations')
    ax1.set_ylabel('Throughput (ops/sec)', color='steelblue')
    ax1.tick_params(axis='y', labelcolor='steelblue')
    
    # Add text labels on bars
    for bar in bars:
        height = bar.get_height()
        ax1.text(bar.get_x() + bar.get_width()/2., height + 5,
                f'{height:.1f}', ha='center', va='bottom')
    
    # Plot normalized throughput (line)
    ax2 = ax1.twinx()
    ax2.plot(np.arange(len(df)), normalized, 'ro-', linewidth=2, markersize=8)
    ax2.set_ylabel('Percent of Peak Throughput (%)', color='red')
    ax2.tick_params(axis='y', labelcolor='red')
    ax2.set_ylim(0, 105)  # 0-100% with a little margin
    
    # Add percentage labels
    for i, val in enumerate(normalized):
        ax2.annotate(f'{val:.1f}%', 
                    (i, val), 
                    textcoords="offset points", 
                    xytext=(0, 5), 
                    ha='center',
                    color='darkred')
    
    plt.title('Throughput vs. Operation Count (Absolute and Relative)')
    plt.tight_layout()
    plt.savefig('visualizations/throughput_normalized.png', dpi=300)
    plt.close()
    
    # 2. Throughput vs Operation Count Scatter with Trend Line
    plt.figure(figsize=(12, 7))
    
    # Create scatter plot
    plt.scatter(df['OperationCount'], df['ThroughputOpsPerSec'], 
               s=100, alpha=0.7, color='blue', edgecolor='black')
    
    # Add trend line with confidence interval
    sns.regplot(x=df['OperationCount'], y=df['ThroughputOpsPerSec'], 
               scatter=False, ci=95, line_kws={"color":"red"})
    
    # Annotate points
    for i, (x, y) in enumerate(zip(df['OperationCount'], df['ThroughputOpsPerSec'])):
        plt.annotate(f"{y:.1f}", (x, y), textcoords="offset points", 
                    xytext=(0, 10), ha='center')
    
    plt.xlabel('Number of Operations')
    plt.ylabel('Throughput (ops/sec)')
    plt.title('Throughput vs. Operation Count with Trend Line')
    plt.grid(True, linestyle='--', alpha=0.7)
    plt.tight_layout()
    plt.savefig('visualizations/throughput_trendline.png', dpi=300)
    plt.close()
    
    # 3. Throughput Efficiency (Throughput per Operation)
    plt.figure(figsize=(12, 7))
    
    # Calculate efficiency (throughput per operation)
    df['Efficiency'] = df['ThroughputOpsPerSec'] / df['OperationCount']
    
    plt.plot(df['OperationCount'], df['Efficiency'], 'go-', linewidth=2, markersize=10)
    
    for i, (x, y) in enumerate(zip(df['OperationCount'], df['Efficiency'])):
        plt.annotate(f"{y:.5f}", (x, y), textcoords="offset points", 
                    xytext=(5, 5), ha='left')
    
    plt.xlabel('Number of Operations')
    plt.ylabel('Efficiency (throughput/op)')
    plt.title('Throughput Efficiency vs. Operation Count')
    plt.grid(True, linestyle='--', alpha=0.7)
    plt.tight_layout()
    plt.savefig('visualizations/throughput_efficiency.png', dpi=300)
    plt.close()
    
    # 4. Throughput vs. Total Time with Execution Time per Operation
    fig, ax1 = plt.subplots(figsize=(12, 7))
    
    # Calculate average time per operation (in milliseconds)
    df['TimePerOp'] = (df['TotalTimeSeconds'] * 1000) / (df['OperationCount'])
    
    # Bar plot for throughput
    bars = ax1.bar(df['OperationCount'].astype(str), df['ThroughputOpsPerSec'], 
                  alpha=0.6, color='steelblue')
    ax1.set_xlabel('Number of Operations')
    ax1.set_ylabel('Throughput (ops/sec)', color='steelblue')
    ax1.tick_params(axis='y', labelcolor='steelblue')
    
    # Line plot for time per operation
    ax2 = ax1.twinx()
    ax2.plot(np.arange(len(df)), df['TimePerOp'], 'mo-', linewidth=2, markersize=8)
    ax2.set_ylabel('Time per Operation (ms)', color='purple')
    ax2.tick_params(axis='y', labelcolor='purple')
    
    # Add percentage labels
    for i, val in enumerate(df['TimePerOp']):
        ax2.annotate(f'{val:.2f} ms', 
                    (i, val), 
                    textcoords="offset points", 
                    xytext=(0, 5), 
                    ha='center',
                    color='darkmagenta')
    
    plt.title('Throughput and Execution Time per Operation')
    plt.tight_layout()
    plt.savefig('visualizations/throughput_time_per_op.png', dpi=300)
    plt.close()
    
    # 5. Radar Chart for Performance Metrics
    # Only create if we have enough metrics
    if 'InterestLatencyMean' in df.columns and 'MaxStashSize' in df.columns:
        # Normalize all metrics to 0-1 scale for radar chart
        metrics = ['ThroughputOpsPerSec', 'InterestLatencyMean', 'DataLatencyMean', 
                  'MaxStashSize', 'TotalTimeSeconds']
        
        # Filter out any metrics that don't exist
        available_metrics = [m for m in metrics if m in df.columns]
        
        if len(available_metrics) >= 3:  # Need at least 3 metrics for a meaningful radar chart
            # Create radar charts for each operation count
            for i, op_count in enumerate(df['OperationCount']):
                # For radar chart, higher is better, so invert latency and stash size
                values = []
                for metric in available_metrics:
                    val = df[metric].iloc[i]
                    # For latency and time, lower is better, so invert the normalization
                    if metric in ['InterestLatencyMean', 'DataLatencyMean', 'MaxStashSize', 'TotalTimeSeconds']:
                        # Find max value safely
                        max_val = df[metric].max() if df[metric].max() > 0 else 1
                        values.append(1 - (val / max_val))
                    else:
                        # For throughput, higher is better
                        max_val = df[metric].max() if df[metric].max() > 0 else 1
                        values.append(val / max_val)
                
                # Create radar chart
                num_metrics = len(available_metrics)
                angles = np.linspace(0, 2*np.pi, num_metrics, endpoint=False).tolist()
                # Close the polygon
                values = values + [values[0]]
                angles = angles + [angles[0]]
                available_metrics_labels = available_metrics + [available_metrics[0]]
                
                # Create plot
                fig, ax = plt.subplots(figsize=(8, 8), subplot_kw=dict(polar=True))
                ax.plot(angles, values, 'o-', linewidth=2)
                ax.fill(angles, values, alpha=0.25)
                ax.set_thetagrids(np.degrees(angles), available_metrics_labels)
                ax.set_ylim(0, 1)
                ax.set_title(f'Performance Profile - {op_count} Operations')
                plt.tight_layout()
                plt.savefig(f'visualizations/radar_profile_{op_count}_ops.png', dpi=300)
                plt.close()
    
    print("Enhanced throughput visualizations saved to visualizations directory")

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
    ax1.set_yscale('log')  # Set logarithmic scale for y-axis
    ax1.grid(True, which="both")  # Grid lines for both major and minor ticks
    # Format y-axis to show actual values instead of powers of 10
    ax1.yaxis.set_major_formatter(ScalarFormatter())
    # Add minor gridlines for better readability in log scale
    ax1.minorticks_on()
    # ax1.grid(True, which='minor', linestyle=':', alpha=0.5)
    # Add this function call at the end of analyze_operations_benchmark
    create_enhanced_throughput_visualizations(df)
    
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