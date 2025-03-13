import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
import seaborn as sns
from matplotlib.gridspec import GridSpec

# Set style
plt.style.use('ggplot')
sns.set_palette("colorblind")
plt.rcParams['font.size'] = 12
plt.rcParams['figure.figsize'] = (12, 8)

def load_and_clean_data(file_path):
    """Load a CSV file and handle potential formatting issues."""
    try:
        # For files with 'Metric,Value' format
        if 'operations_' in file_path or 'baseline_' in file_path or 'privacy_' in file_path or 'config_th' in file_path:
            df = pd.read_csv(file_path)
            # Convert to key-value pairs if needed
            if 'Metric' in df.columns and 'Value' in df.columns:
                # Extract metrics into a dictionary
                metrics = {}
                for _, row in df.iterrows():
                    try:
                        # Try to convert to float if possible
                        metrics[row['Metric']] = float(row['Value'])
                    except (ValueError, TypeError):
                        # Keep as string if not convertible
                        metrics[row['Metric']] = row['Value']
                return pd.Series(metrics)
            return df
        # For other formats
        else:
            return pd.read_csv(file_path)
    except Exception as e:
        print(f"Error loading {file_path}: {e}")
        return None

def plot_baseline_comparison(baseline_comp_path='baseline_comparison.csv'):
    """Plot comparison between baseline and privacy implementations."""
    print(f"Plotting baseline comparison from: {baseline_comp_path}")
    
    # Load data
    df = load_and_clean_data(baseline_comp_path)
    if df is None or df.empty:
        print("No data found for baseline comparison")
        return
    
    fig = plt.figure(figsize=(16, 10))
    gs = GridSpec(2, 3, figure=fig)
    
    # Plot 1: Throughput comparison
    ax1 = fig.add_subplot(gs[0, 0])
    x = df['OperationCount'].astype(str)
    width = 0.35
    ax1.bar(np.arange(len(x)) - width/2, df['BaselineThroughput'], width, label='Baseline')
    ax1.bar(np.arange(len(x)) + width/2, df['PrivacyThroughput'], width, label='Privacy')
    ax1.set_xlabel('Operation Count')
    ax1.set_ylabel('Throughput (ops/sec)')
    ax1.set_title('Throughput Comparison')
    ax1.set_xticks(np.arange(len(x)))
    ax1.set_xticklabels(x)
    ax1.legend()
    ax1.set_yscale('log')  # Log scale due to large differences
    
    # Plot 2: Latency comparisons
    ax2 = fig.add_subplot(gs[0, 1])
    metrics = ['InterestLatency', 'DataLatency', 'RetrievalLatency']
    
    width = 0.35
    ind = np.arange(len(metrics))
    
    # Use the first operation count (usually smaller for better visibility)
    op_idx = 0
    ax2.bar(ind - width/2, [df[f'Baseline{m}'].iloc[op_idx] for m in metrics], width, label='Baseline')
    ax2.bar(ind + width/2, [df[f'Privacy{m}'].iloc[op_idx] for m in metrics], width, label='Privacy')
    
    ax2.set_xlabel('Latency Type')
    ax2.set_ylabel('Latency (μs)')
    ax2.set_title(f'Latency Comparison (Op Count: {df["OperationCount"].iloc[op_idx]})')
    ax2.set_xticks(ind)
    ax2.set_xticklabels(metrics)
    ax2.legend()
    
    # Plot 3: Overhead factors
    ax3 = fig.add_subplot(gs[0, 2])
    overhead_metrics = ['ThroughputOverhead', 'InterestLatencyOverhead', 
                         'DataLatencyOverhead', 'RetrievalLatencyOverhead']
    overhead_labels = ['Throughput', 'Interest\nLatency', 'Data\nLatency', 'Retrieval\nLatency']
    
    for i, op_count in enumerate(df['OperationCount']):
        values = [df[metric].iloc[i] for metric in overhead_metrics]
        ax3.bar(np.arange(len(overhead_metrics)) + i*width, values, width, 
                label=f'{op_count} ops')
    
    ax3.set_xlabel('Metric')
    ax3.set_ylabel('Overhead Factor (×)')
    ax3.set_title('Performance Overhead Factors')
    ax3.set_xticks(np.arange(len(overhead_metrics)) + width/2)
    ax3.set_xticklabels(overhead_labels)
    ax3.legend()
    
    # Plot 4: Memory usage
    ax4 = fig.add_subplot(gs[1, 0])
    ax4.bar(np.arange(len(x)) - width/2, df['BaselineMemoryMB'], width, label='Baseline')
    ax4.bar(np.arange(len(x)) + width/2, df['PrivacyMemoryMB'], width, label='Privacy')
    ax4.set_xlabel('Operation Count')
    ax4.set_ylabel('Memory Usage (MB)')
    ax4.set_title('Memory Usage Comparison')
    ax4.set_xticks(np.arange(len(x)))
    ax4.set_xticklabels(x)
    ax4.legend()
    
    # Plot 5: Detailed latency for higher operation count
    if len(df) > 1:
        ax5 = fig.add_subplot(gs[1, 1])
        op_idx = 1  # Use the larger operation count
        ax5.bar(ind - width/2, [df[f'Baseline{m}'].iloc[op_idx] for m in metrics], width, label='Baseline')
        ax5.bar(ind + width/2, [df[f'Privacy{m}'].iloc[op_idx] for m in metrics], width, label='Privacy')
        
        ax5.set_xlabel('Latency Type')
        ax5.set_ylabel('Latency (μs)')
        ax5.set_title(f'Latency Comparison (Op Count: {df["OperationCount"].iloc[op_idx]})')
        ax5.set_xticks(ind)
        ax5.set_xticklabels(metrics)
        ax5.legend()
    
    plt.tight_layout()
    plt.savefig('baseline_comparison.png', dpi=300, bbox_inches='tight')
    plt.close()
    
    print("Baseline comparison plot saved as 'baseline_comparison.png'")

def plot_operations_benchmark(benchmark_path='operations_benchmark.csv'):
    """Plot the impact of operation count on performance metrics."""
    print(f"Plotting operations benchmark from: {benchmark_path}")
    
    # Load data
    df = load_and_clean_data(benchmark_path)
    if df is None or df.empty:
        print("No data found for operations benchmark")
        return
    
    fig, axes = plt.subplots(2, 2, figsize=(14, 10))
    
    # Plot 1: Throughput vs Operation Count
    axes[0, 0].plot(df['OperationCount'], df['ThroughputOpsPerSec'], 'o-', linewidth=2)
    axes[0, 0].set_xlabel('Operation Count')
    axes[0, 0].set_ylabel('Throughput (ops/sec)')
    axes[0, 0].set_title('Throughput vs Operation Count')
    axes[0, 0].set_xscale('log')
    
    # Plot 2: Latency vs Operation Count
    ax = axes[0, 1]
    ax.plot(df['OperationCount'], df['InterestLatencyMean'], 'o-', linewidth=2, label='Interest')
    ax.plot(df['OperationCount'], df['DataLatencyMean'], 's-', linewidth=2, label='Data')
    ax.plot(df['OperationCount'], df['RetrievalLatencyMean'], '^-', linewidth=2, label='Retrieval')
    ax.set_xlabel('Operation Count')
    ax.set_ylabel('Latency (μs)')
    ax.set_title('Latency vs Operation Count')
    ax.set_xscale('log')
    ax.legend()
    
    # Plot 3: Max Stash Size vs Operation Count
    axes[1, 0].plot(df['OperationCount'], df['MaxStashSize'], 'o-', linewidth=2)
    axes[1, 0].set_xlabel('Operation Count')
    axes[1, 0].set_ylabel('Max Stash Size')
    axes[1, 0].set_title('Max Stash Size vs Operation Count')
    axes[1, 0].set_xscale('log')
    
    # Plot 4: Total Time vs Operation Count
    axes[1, 1].plot(df['OperationCount'], df['TotalTimeSeconds'], 'o-', linewidth=2)
    axes[1, 1].set_xlabel('Operation Count')
    axes[1, 1].set_ylabel('Total Time (seconds)')
    axes[1, 1].set_title('Total Time vs Operation Count')
    axes[1, 1].set_xscale('log')
    axes[1, 1].set_yscale('log')
    
    plt.tight_layout()
    plt.savefig('operations_benchmark.png', dpi=300, bbox_inches='tight')
    plt.close()
    
    print("Operations benchmark plot saved as 'operations_benchmark.png'")

def plot_config_parameters(config_benchmark_path='config_benchmark_results.csv'):
    """Plot the impact of different configuration parameters on performance."""
    print(f"Plotting configuration parameters from: {config_benchmark_path}")
    
    # Load data
    df = load_and_clean_data(config_benchmark_path)
    if df is None or df.empty:
        print("No data found for configuration benchmark")
        return
    
    # Remove error rows
    df = df[~df['Throughput'].astype(str).str.contains('ERROR')]
    df['Throughput'] = pd.to_numeric(df['Throughput'], errors='coerce')
    df = df.dropna(subset=['Throughput'])
    
    if df.empty:
        print("No valid data points in configuration benchmark")
        return
    
    # Create figure
    fig = plt.figure(figsize=(16, 12))
    gs = GridSpec(2, 3, figure=fig)
    
    # Plot 1: Impact of Tree Height on Throughput
    tree_height_df = df[(df['BucketCapacity'] == 4) & (df['StashLimit'] == 100)]
    if not tree_height_df.empty:
        ax1 = fig.add_subplot(gs[0, 0])
        ax1.plot(tree_height_df['TreeHeight'], tree_height_df['Throughput'], 'o-', linewidth=2)
        ax1.set_xlabel('Tree Height')
        ax1.set_ylabel('Throughput (ops/sec)')
        ax1.set_title('Tree Height vs Throughput', pad=20)
        # Set integer ticks for x-axis
        ax1.set_xticks(tree_height_df['TreeHeight'].unique())
        
        # Add second y-axis for latency
        ax1_twin = ax1.twinx()
        ax1_twin.plot(tree_height_df['TreeHeight'], tree_height_df['AvgInterestLatency'], 's--', color='red', linewidth=2)
        ax1_twin.set_ylabel('Avg Interest Latency (μs)', color='red')
        ax1_twin.tick_params(axis='y', labelcolor='red')
    
    # Plot 2: Impact of Bucket Capacity on Throughput
    bucket_capacity_df = df[(df['TreeHeight'] == 5) & (df['StashLimit'] == 100)]
    if not bucket_capacity_df.empty:
        ax2 = fig.add_subplot(gs[0, 1])
        ax2.plot(bucket_capacity_df['BucketCapacity'], bucket_capacity_df['Throughput'], 'o-', linewidth=2)
        ax2.set_xlabel('Bucket Capacity')
        ax2.set_ylabel('Throughput (ops/sec)')
        ax2.set_title('Bucket Capacity vs Throughput', pad=20)
        # Set integer ticks for x-axis
        ax2.set_xticks(bucket_capacity_df['BucketCapacity'].unique())
        
        # Add second y-axis for latency
        ax2_twin = ax2.twinx()
        ax2_twin.plot(bucket_capacity_df['BucketCapacity'], bucket_capacity_df['AvgInterestLatency'], 's--', color='red', linewidth=2)
        ax2_twin.set_ylabel('Avg Interest Latency (μs)', color='red')
        ax2_twin.tick_params(axis='y', labelcolor='red')
    
    # Plot 3: Impact of Stash Limit on Throughput
    stash_limit_df = df[(df['TreeHeight'] == 5) & (df['BucketCapacity'] == 4)]
    if not stash_limit_df.empty:
        ax3 = fig.add_subplot(gs[0, 2])
        ax3.plot(stash_limit_df['StashLimit'], stash_limit_df['Throughput'], 'o-', linewidth=2)
        ax3.set_xlabel('Stash Limit')
        ax3.set_ylabel('Throughput (ops/sec)')
        ax3.set_title('Stash Limit vs Throughput', pad=20)
        
        # Add second y-axis for max stash size
        ax3_twin = ax3.twinx()
        ax3_twin.plot(stash_limit_df['StashLimit'], stash_limit_df['MaxStashSize'], 's--', color='green', linewidth=2)
        ax3_twin.set_ylabel('Max Stash Size', color='green')
        ax3_twin.tick_params(axis='y', labelcolor='green')
    
    # Plot 4: Tree Height vs Latency Components
    if not tree_height_df.empty:
        ax4 = fig.add_subplot(gs[1, 0])
        ax4.plot(tree_height_df['TreeHeight'], tree_height_df['AvgInterestLatency'], 'o-', linewidth=2, label='Interest')
        ax4.plot(tree_height_df['TreeHeight'], tree_height_df['AvgDataLatency'], 's-', linewidth=2, label='Data')
        ax4.plot(tree_height_df['TreeHeight'], tree_height_df['AvgRetrievalLatency'], '^-', linewidth=2, label='Retrieval')
        ax4.set_xlabel('Tree Height')
        ax4.set_ylabel('Latency (μs)')
        ax4.set_title('Tree Height: Latency Components', pad=20)
        ax4.set_xticks(tree_height_df['TreeHeight'].unique())
        ax4.legend()
    
    # Plot 5: Bucket Capacity vs Latency Components
    if not bucket_capacity_df.empty:
        ax5 = fig.add_subplot(gs[1, 1])
        ax5.plot(bucket_capacity_df['BucketCapacity'], bucket_capacity_df['AvgInterestLatency'], 'o-', linewidth=2, label='Interest')
        ax5.plot(bucket_capacity_df['BucketCapacity'], bucket_capacity_df['AvgDataLatency'], 's-', linewidth=2, label='Data')
        ax5.plot(bucket_capacity_df['BucketCapacity'], bucket_capacity_df['AvgRetrievalLatency'], '^-', linewidth=2, label='Retrieval')
        ax5.set_xlabel('Bucket Capacity')
        ax5.set_ylabel('Latency (μs)')
        ax5.set_title('Bucket Capacity: Latency Components', pad=20)
        ax5.set_xticks(bucket_capacity_df['BucketCapacity'].unique())
        ax5.legend()
    
    # Plot 6: Stash Limit vs Latency Components
    if not stash_limit_df.empty:
        ax6 = fig.add_subplot(gs[1, 2])
        ax6.plot(stash_limit_df['StashLimit'], stash_limit_df['AvgInterestLatency'], 'o-', linewidth=2, label='Interest')
        ax6.plot(stash_limit_df['StashLimit'], stash_limit_df['AvgDataLatency'], 's-', linewidth=2, label='Data')
        ax6.plot(stash_limit_df['StashLimit'], stash_limit_df['AvgRetrievalLatency'], '^-', linewidth=2, label='Retrieval')
        ax6.set_xlabel('Stash Limit')
        ax6.set_ylabel('Latency (μs)')
        ax6.set_title('Stash Limit: Latency Components', pad=20)
        ax6.legend()
    
    plt.tight_layout()
    plt.savefig('config_parameters.png', dpi=300, bbox_inches='tight')
    plt.close()
    
    print("Configuration parameters plot saved as 'config_parameters.png'")

def plot_config_details():
    """Plot details of different configurations from individual config files."""
    print("Plotting configuration details from individual files")
    
    # List of config files to analyze
    config_files = [
        'config_th5_bc4_sl100.csv',
        'config_th5_bc4_sl200.csv',
        'config_th5_bc4_sl500.csv',
        'config_th5_bc8_sl100.csv',
        'config_th5_bc16_sl100.csv',
        'config_th6_bc4_sl100.csv',
        'config_th7_bc4_sl100.csv'
    ]
    
    # Extract configuration parameters from filenames
    configs = []
    for file in config_files:
        parts = file.replace('.csv', '').split('_')
        # Check if filename follows expected pattern
        if len(parts) >= 4 and parts[1].startswith('th') and parts[2].startswith('bc') and parts[3].startswith('sl'):
            config = {
                'file': file,
                'tree_height': int(parts[1][2:]),
                'bucket_capacity': int(parts[2][2:]),
                'stash_limit': int(parts[3][2:])
            }
            
            # Load the file data
            series = load_and_clean_data(file)
            if series is not None:
                for metric in ['InterestLatencyMean', 'DataLatencyMean', 'RetrievalLatencyMean', 
                               'Throughput', 'MaxStashSize', 'AvgStashSize']:
                    if metric in series:
                        config[metric] = series[metric]
            
            configs.append(config)
    
    if not configs:
        print("No valid configuration files found or processed")
        return
    
    # Create a DataFrame from the extracted configs
    config_df = pd.DataFrame(configs)
    
    # Create a unique identifier for each configuration
    config_df['config_id'] = config_df.apply(
        lambda row: f"TH{row['tree_height']}-BC{row['bucket_capacity']}-SL{row['stash_limit']}", 
        axis=1
    )
    
    # Plot the results
    fig, axes = plt.subplots(2, 2, figsize=(16, 12))
    
    # Plot 1: Throughput comparison
    if 'Throughput' in config_df.columns:
        sns.barplot(x='config_id', y='Throughput', data=config_df, ax=axes[0, 0], palette='viridis')
        axes[0, 0].set_xlabel('Configuration')
        axes[0, 0].set_ylabel('Throughput (ops/sec)')
        axes[0, 0].set_title('Throughput Comparison Across Configurations')
        axes[0, 0].set_xticklabels(axes[0, 0].get_xticklabels(), rotation=45, ha='right')
    
    # Plot 2: Latency comparison
    latency_cols = [col for col in ['InterestLatencyMean', 'DataLatencyMean', 'RetrievalLatencyMean'] 
                   if col in config_df.columns]
    
    if latency_cols:
        # Reshape for grouped bar plot
        latency_data = []
        for _, row in config_df.iterrows():
            for col in latency_cols:
                if pd.notna(row[col]):
                    latency_data.append({
                        'config_id': row['config_id'],
                        'latency_type': col.replace('LatencyMean', ''),
                        'value': row[col]
                    })
        
        latency_df = pd.DataFrame(latency_data)
        if not latency_df.empty:
            sns.barplot(x='config_id', y='value', hue='latency_type', data=latency_df, ax=axes[0, 1], palette='muted')
            axes[0, 1].set_xlabel('Configuration')
            axes[0, 1].set_ylabel('Latency (μs)')
            axes[0, 1].set_title('Latency Comparison Across Configurations')
            axes[0, 1].set_xticklabels(axes[0, 1].get_xticklabels(), rotation=45, ha='right')
            axes[0, 1].legend(title='Latency Type')
    
    # Plot 3: Stash size comparison
    stash_cols = [col for col in ['MaxStashSize', 'AvgStashSize'] if col in config_df.columns]
    
    if stash_cols:
        # Reshape for grouped bar plot
        stash_data = []
        for _, row in config_df.iterrows():
            for col in stash_cols:
                if pd.notna(row[col]):
                    stash_data.append({
                        'config_id': row['config_id'],
                        'stash_metric': col.replace('StashSize', ' Stash Size'),
                        'value': row[col]
                    })
        
        stash_df = pd.DataFrame(stash_data)
        if not stash_df.empty:
            sns.barplot(x='config_id', y='value', hue='stash_metric', data=stash_df, ax=axes[1, 0], palette='Set2')
            axes[1, 0].set_xlabel('Configuration')
            axes[1, 0].set_ylabel('Stash Size')
            axes[1, 0].set_title('Stash Size Comparison Across Configurations')
            axes[1, 0].set_xticklabels(axes[1, 0].get_xticklabels(), rotation=45, ha='right')
            axes[1, 0].legend(title='Metric')
    
    # Plot 4: Heatmap of configuration parameters and performance
    # Create a normalized version of key metrics for better visualization
    if not config_df.empty and 'Throughput' in config_df.columns:
        metrics_to_normalize = [col for col in ['Throughput', 'InterestLatencyMean', 'DataLatencyMean', 
                                              'RetrievalLatencyMean', 'MaxStashSize'] 
                               if col in config_df.columns]
        
        if metrics_to_normalize:
            norm_df = config_df.copy()
            for col in metrics_to_normalize:
                if col == 'Throughput':
                    # Higher throughput is better
                    norm_df[f'{col}_norm'] = config_df[col] / config_df[col].max()
                else:
                    # Lower latency/stash size is better
                    norm_df[f'{col}_norm'] = 1 - (config_df[col] / config_df[col].max())
            
            # Create a heatmap of normalized metrics
            norm_cols = [f'{col}_norm' for col in metrics_to_normalize]
            heatmap_df = norm_df.set_index('config_id')[norm_cols]
            heatmap_df.columns = [col.replace('_norm', '') for col in heatmap_df.columns]
            
            if not heatmap_df.empty:
                sns.heatmap(heatmap_df, annot=True, cmap='RdYlGn', ax=axes[1, 1])
                axes[1, 1].set_title('Performance Metrics Across Configurations (Normalized)')
                axes[1, 1].set_ylabel('Configuration')
                # Higher values (green) indicate better performance
                axes[1, 1].text(1.05, 0.5, "Higher is better", rotation=90, va='center', transform=axes[1, 1].transAxes)
    
    plt.tight_layout()
    plt.savefig('config_details.png', dpi=300, bbox_inches='tight')
    plt.close()
    
    print("Configuration details plot saved as 'config_details.png'")

def plot_trade_off_analysis():
    """Plot trade-off analysis between parameters."""
    print("Plotting trade-off analysis")
    
    # Load benchmark results
    df = load_and_clean_data('config_benchmark_results.csv')
    if df is None or df.empty:
        print("No data found for trade-off analysis")
        return
    
    # Remove error rows
    df = df[~df['Throughput'].astype(str).str.contains('ERROR')]
    df['Throughput'] = pd.to_numeric(df['Throughput'], errors='coerce')
    df = df.dropna(subset=['Throughput'])
    
    if df.empty:
        print("No valid data points for trade-off analysis")
        return
    
    # Create figure
    fig, axes = plt.subplots(2, 2, figsize=(14, 12))
    
    # Plot 1: Tree Height vs Throughput (different bucket capacities)
    ax1 = axes[0, 0]
    bucket_sizes = df['BucketCapacity'].unique()
    for bc in bucket_sizes:
        subset = df[(df['BucketCapacity'] == bc) & (df['StashLimit'] == 100)]
        if not subset.empty:
            ax1.plot(subset['TreeHeight'], subset['Throughput'], 'o-', 
                     linewidth=2, label=f'BC={bc}')
    
    ax1.set_xlabel('Tree Height')
    ax1.set_ylabel('Throughput (ops/sec)')
    ax1.set_title('Tree Height vs Throughput (Different Bucket Capacities)')
    if len(bucket_sizes) > 1:
        ax1.legend(title='Bucket Capacity')
    
    # Plot 2: Tree Height vs Interest Latency (different bucket capacities)
    ax2 = axes[0, 1]
    for bc in bucket_sizes:
        subset = df[(df['BucketCapacity'] == bc) & (df['StashLimit'] == 100)]
        if not subset.empty:
            ax2.plot(subset['TreeHeight'], subset['AvgInterestLatency'], 'o-', 
                     linewidth=2, label=f'BC={bc}')
    
    ax2.set_xlabel('Tree Height')
    ax2.set_ylabel('Average Interest Latency (μs)')
    ax2.set_title('Tree Height vs Interest Latency (Different Bucket Capacities)')
    if len(bucket_sizes) > 1:
        ax2.legend(title='Bucket Capacity')
    
    # Plot 3: Throughput vs Latency scatter plot
    ax3 = axes[1, 0]
    scatter = ax3.scatter(df['Throughput'], df['AvgInterestLatency'], 
                         c=df['TreeHeight'], cmap='viridis', s=100, alpha=0.7)
    
    # Add labels to points
    for i, row in df.iterrows():
        ax3.annotate(f"TH{row['TreeHeight']},BC{row['BucketCapacity']}",
                    (row['Throughput'], row['AvgInterestLatency']),
                    xytext=(5, 5), textcoords='offset points',
                    fontsize=8)
    
    ax3.set_xlabel('Throughput (ops/sec)')
    ax3.set_ylabel('Average Interest Latency (μs)')
    ax3.set_title('Throughput vs Latency Trade-off')
    cbar = plt.colorbar(scatter, ax=ax3)
    cbar.set_label('Tree Height')
    
    # Plot 4: Stash Limit impact on throughput and max stash size
    stash_df = df[df['TreeHeight'] == 5][df['BucketCapacity'] == 4]
    if not stash_df.empty:
        ax4 = axes[1, 1]
        ax4.plot(stash_df['StashLimit'], stash_df['Throughput'], 'o-', color='blue', linewidth=2)
        ax4.set_xlabel('Stash Limit')
        ax4.set_ylabel('Throughput (ops/sec)', color='blue')
        ax4.tick_params(axis='y', labelcolor='blue')
        
        ax4_twin = ax4.twinx()
        ax4_twin.plot(stash_df['StashLimit'], stash_df['MaxStashSize'], 's--', color='red', linewidth=2)
        ax4_twin.set_ylabel('Max Stash Size', color='red')
        ax4_twin.tick_params(axis='y', labelcolor='red')
        
        ax4.set_title('Impact of Stash Limit on Throughput and Max Stash Size')
    
    plt.tight_layout()
    plt.savefig('trade_off_analysis.png', dpi=300, bbox_inches='tight')
    plt.close()
    
    print("Trade-off analysis plot saved as 'trade_off_analysis.png'")

def main():
    """Main function to run all visualization functions."""
    print("Starting NDN Router Performance Visualization")
    
    plot_baseline_comparison()
    plot_operations_benchmark()
    plot_config_parameters()
    plot_config_details()
    plot_trade_off_analysis()
    
    print("All visualizations completed successfully!")

if __name__ == "__main__":
    main()