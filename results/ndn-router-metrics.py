import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
import seaborn as sns
from matplotlib.ticker import ScalarFormatter
import os

def load_metric_data(filepath):
    """Load and organize the router metrics from CSV file"""
    # Read the CSV as a simple two-column dataset
    df = pd.read_csv(filepath)
    
    # Extract aggregated metrics (key-value pairs at the top of the file)
    aggregated_metrics = {}
    for _, row in df.iterrows():
        if pd.notna(row['Metric']) and pd.notna(row['Value']):
            aggregated_metrics[row['Metric']] = row['Value']
        if row['Metric'] == 'PeakMemoryUsageMB':
            # This is typically the last row before raw data
            break
    
    # Extract raw latency data
    raw_data = {'interest': [], 'data': [], 'retrieval': []}
    current_section = None
    
    # Re-read the file to handle the raw data sections properly
    with open(filepath, 'r') as f:
        lines = f.readlines()
        
    for line in lines:
        line = line.strip()
        if 'Raw Interest Latencies' in line:
            current_section = 'interest'
            continue
        elif 'Raw Data Latencies' in line:
            current_section = 'data'
            continue
        elif 'Raw Retrieval Latencies' in line:
            current_section = 'retrieval'
            continue
        
        if current_section and line and line[0].isdigit():
            try:
                value = float(line)
                raw_data[current_section].append(value)
            except ValueError:
                pass
    
    return {
        'aggregated': aggregated_metrics,
        'raw': raw_data
    }

def plot_bar_comparison(baseline_metrics, oblivious_metrics, metric_names, title, ylabel, log_scale=False):
    """Create a bar chart comparing metrics between baseline and oblivious routers"""
    fig, ax = plt.subplots(figsize=(10, 6))
    
    x = np.arange(len(metric_names))
    width = 0.35
    
    baseline_values = [baseline_metrics[metric] for metric in metric_names]
    oblivious_values = [oblivious_metrics[metric] for metric in metric_names]
    
    rects1 = ax.bar(x - width/2, baseline_values, width, label='Baseline Router', color='#3498db')
    rects2 = ax.bar(x + width/2, oblivious_values, width, label='Oblivious Router', color='#e74c3c')
    
    # Set a reasonable y-limit if using log scale to prevent excessive figure height
    if log_scale and min(baseline_values + oblivious_values) > 0:
        ax.set_yscale('log')
        ax.yaxis.set_major_formatter(ScalarFormatter())
        
        # Set upper limit to avoid excessive vertical size
        max_val = max(baseline_values + oblivious_values)
        min_val = min(val for val in baseline_values + oblivious_values if val > 0)
        ax.set_ylim(min_val * 0.5, max_val * 5)  # Reasonable padding
    
    # Add text labels above bars with controlled positioning
    for i, rect in enumerate(rects1):
        height = rect.get_height()
        if height <= 0:  # Skip labels for zero or negative values
            continue
            
        value_text = f"{baseline_values[i]:.2f}"
        if log_scale:
            # Position text at a fixed ratio above bar in log scale
            y_pos = min(height * 1.5, ax.get_ylim()[1] * 0.9)
        else:
            # Position text at fixed offset above bar in linear scale
            y_pos = height + 0.05 * (ax.get_ylim()[1] - ax.get_ylim()[0])
            
        ax.text(rect.get_x() + rect.get_width()/2., y_pos,
                value_text, ha='center', va='bottom', fontsize=9, rotation=45)
    
    for i, rect in enumerate(rects2):
        height = rect.get_height()
        if height <= 0:  # Skip labels for zero or negative values
            continue
            
        value_text = f"{oblivious_values[i]:.2f}"
        if log_scale:
            # Position text at a fixed ratio above bar in log scale
            y_pos = min(height * 1.5, ax.get_ylim()[1] * 0.9)
        else:
            # Position text at fixed offset above bar in linear scale
            y_pos = height + 0.05 * (ax.get_ylim()[1] - ax.get_ylim()[0])
            
        ax.text(rect.get_x() + rect.get_width()/2., y_pos,
                value_text, ha='center', va='bottom', fontsize=9, rotation=45)
    
    # Calculate and display the ratio text
    for i in range(len(metric_names)):
        if baseline_values[i] > 0:  # Avoid division by zero
            ratio = oblivious_values[i] / baseline_values[i]
            if ratio > 1:
                ratio_text = f"{ratio:.2f}x slower"
            else:
                ratio_text = f"{1/ratio:.2f}x faster"
            
            # Position at bottom of chart
            y_pos = ax.get_ylim()[0] * 1.1 if log_scale else ax.get_ylim()[0]
            ax.text(i, y_pos, ratio_text, ha='center', va='bottom', fontsize=8,
                   bbox=dict(facecolor='white', alpha=0.7, boxstyle='round,pad=0.3'))
    
    ax.set_ylabel(ylabel)
    ax.set_title(title)
    ax.set_xticks(x)
    ax.set_xticklabels(metric_names, rotation=45, ha='right')
    ax.legend()
    
    fig.tight_layout()
    return fig

def plot_throughput_comparison(baseline_metrics, oblivious_metrics):
    """Create a special bar chart for throughput comparison"""
    fig, ax = plt.subplots(figsize=(8, 6))
    
    labels = ['Baseline Router', 'Oblivious Router']
    values = [baseline_metrics['Throughput'], oblivious_metrics['Throughput']]
    
    bars = ax.bar(labels, values, color=['#3498db', '#e74c3c'])
    
    # Add text labels above bars
    for bar in bars:
        height = bar.get_height()
        ax.text(bar.get_x() + bar.get_width()/2., height * 1.01,
                f'{height:.2f} ops/s', ha='center', va='bottom')
    
    # Calculate slowdown factor
    slowdown = baseline_metrics['Throughput'] / oblivious_metrics['Throughput']
    ax.text(1, values[1] * 0.5, f"{slowdown:.2f}x slower", ha='center', 
           bbox=dict(facecolor='white', alpha=0.7, boxstyle='round,pad=0.3'))
    
    ax.set_ylabel('Operations per Second')
    ax.set_title('Throughput Comparison')
    ax.set_ylim(0, max(values) * 1.2)
    
    # Add log scale inset for better visualization
    axins = ax.inset_axes([0.55, 0.55, 0.4, 0.4])
    axins.bar(labels, values, color=['#3498db', '#e74c3c'])
    axins.set_yscale('log')
    axins.set_title('Log Scale', fontsize=9)
    for i, v in enumerate(values):
        axins.text(i, v * 1.1, f'{v:.2f}', ha='center', fontsize=8)
    
    fig.tight_layout()
    return fig

def plot_latency_distribution(baseline_data, oblivious_data, latency_type, title):
    """Create a density plot comparing the latency distributions"""
    fig, ax = plt.subplots(figsize=(10, 6))
    
    # Handle potential extreme outliers that could cause rendering issues
    # Use percentiles to filter out extreme outliers for visualization purposes
    baseline_filtered = np.array(baseline_data)
    oblivious_filtered = np.array(oblivious_data)
    
    # For very skewed distributions, limit to 99.5th percentile for visualization
    if len(baseline_filtered) > 10:
        baseline_upper = np.percentile(baseline_filtered, 99.5)
        baseline_filtered = baseline_filtered[baseline_filtered <= baseline_upper]
    
    if len(oblivious_filtered) > 10:
        oblivious_upper = np.percentile(oblivious_filtered, 99.5)
        oblivious_filtered = oblivious_filtered[oblivious_filtered <= oblivious_upper]
    
    # Convert to DataFrames for easier plotting
    df_baseline = pd.DataFrame({
        'Latency': baseline_filtered,
        'Router': 'Baseline'
    })
    
    df_oblivious = pd.DataFrame({
        'Latency': oblivious_filtered,
        'Router': 'Oblivious'
    })
    
    df_combined = pd.concat([df_baseline, df_oblivious])
    
    # Plot KDE for both distributions
    try:
        sns.kdeplot(data=df_combined, x='Latency', hue='Router', fill=True, 
                    common_norm=False, palette=['#3498db', '#e74c3c'], alpha=0.5, ax=ax)
    except Exception as e:
        print(f"Warning: KDE plot failed for {latency_type}, using histograms instead. Error: {e}")
        # Fallback to histograms if KDE fails
        ax.hist(baseline_filtered, alpha=0.5, color='#3498db', label='Baseline', bins=20, density=True)
        ax.hist(oblivious_filtered, alpha=0.5, color='#e74c3c', label='Oblivious', bins=20, density=True)
    
    # Calculate statistics on original (unfiltered) data
    baseline_mean = np.mean(baseline_data)
    oblivious_mean = np.mean(oblivious_data)
    
    # Set reasonable x-axis limits to avoid rendering issues
    if max(oblivious_data) / max(baseline_data) > 10:
        ax.set_xscale('log')
        ax.xaxis.set_major_formatter(ScalarFormatter())
        
        # Set sensible x limits based on filtered data
        min_val = min(min(baseline_filtered), min(oblivious_filtered))
        max_val = max(max(baseline_filtered), max(oblivious_filtered))
        ax.set_xlim(min_val * 0.5, max_val * 2)
    else:
        # For non-log scale, set limits based on the 99.9th percentile
        max_val = max(
            np.percentile(baseline_filtered, 99.9) if len(baseline_filtered) > 0 else 0,
            np.percentile(oblivious_filtered, 99.9) if len(oblivious_filtered) > 0 else 0
        )
        ax.set_xlim(0, max_val * 1.1)
    
    # Add vertical lines for means (of original data)
    ax.axvline(baseline_mean, color='#3498db', linestyle='--', 
               label=f'Baseline Mean: {baseline_mean:.2f} μs')
    ax.axvline(oblivious_mean, color='#e74c3c', linestyle='--', 
               label=f'Oblivious Mean: {oblivious_mean:.2f} μs')
    
    # Update legend to include mean lines
    handles, labels = ax.get_legend_handles_labels()
    ax.legend(handles=handles, labels=labels, title='Router Type')
    
    # Add annotation showing the difference
    if baseline_mean > 0:  # Avoid division by zero
        ratio = oblivious_mean / baseline_mean
        ratio_text = f"Oblivious is {ratio:.2f}x slower"
        
        # Position the text in an appropriate spot within current axis limits
        y_pos = ax.get_ylim()[1] * 0.9
        x_pos = ax.get_xlim()[0] + (ax.get_xlim()[1] - ax.get_xlim()[0]) * 0.5
        
        ax.text(x_pos, y_pos, ratio_text, ha='center', va='top',
                bbox=dict(facecolor='white', alpha=0.7, boxstyle='round,pad=0.3'))
    
    ax.set_xlabel('Latency (μs)')
    ax.set_ylabel('Density')
    ax.set_title(title)
    
    fig.tight_layout()
    return fig

def plot_latency_boxplot(baseline_data, oblivious_data, latency_type, title):
    """Create a box plot comparing the latency distributions"""
    fig, ax = plt.subplots(figsize=(10, 6))
    
    # Filter extreme outliers for visualization purposes (keep all data for statistics)
    baseline_filtered = np.array(baseline_data)
    oblivious_filtered = np.array(oblivious_data)
    
    # For very skewed distributions, limit to 99th percentile for visualization
    if len(baseline_filtered) > 10:
        baseline_upper = np.percentile(baseline_filtered, 99)
        baseline_filtered = baseline_filtered[baseline_filtered <= baseline_upper]
    
    if len(oblivious_filtered) > 10:
        oblivious_upper = np.percentile(oblivious_filtered, 99)
        oblivious_filtered = oblivious_filtered[oblivious_filtered <= oblivious_upper]
    
    # Prepare data for plotting
    df = pd.DataFrame({
        'Baseline': baseline_filtered,
        'Oblivious': oblivious_filtered
    })
    
    # Create boxplot
    sns.boxplot(data=df, palette=['#3498db', '#e74c3c'], ax=ax)
    
    # Add a more detailed swarm plot on top (limit points for clarity)
    # Take a small random sample to avoid overcrowding
    if len(baseline_filtered) > 0:
        baseline_sample = np.random.choice(baseline_filtered, min(30, len(baseline_filtered)), replace=False)
    else:
        baseline_sample = []
        
    if len(oblivious_filtered) > 0:
        oblivious_sample = np.random.choice(oblivious_filtered, min(30, len(oblivious_filtered)), replace=False)
    else:
        oblivious_sample = []
    
    if len(baseline_sample) > 0 or len(oblivious_sample) > 0:
        sample_df = pd.DataFrame({
            'Router': ['Baseline'] * len(baseline_sample) + ['Oblivious'] * len(oblivious_sample),
            'Latency': np.concatenate([baseline_sample, oblivious_sample])
        })
        
        sns.swarmplot(x='Router', y='Latency', data=sample_df, color='black', alpha=0.5, ax=ax)
    
    # Calculate statistics on the original data (not filtered)
    baseline_mean = np.mean(baseline_data)
    oblivious_mean = np.mean(oblivious_data)
    baseline_median = np.median(baseline_data)
    oblivious_median = np.median(oblivious_data)
    
    stats_text = (
        f"Baseline - Mean: {baseline_mean:.2f} μs, Median: {baseline_median:.2f} μs\n"
        f"Oblivious - Mean: {oblivious_mean:.2f} μs, Median: {oblivious_median:.2f} μs\n"
    )
    
    if baseline_mean > 0:  # Avoid division by zero
        ratio = oblivious_mean / baseline_mean
        stats_text += f"Mean Ratio: Oblivious is {ratio:.2f}x slower"
    
    ax.text(0.5, 0.01, stats_text, transform=ax.transAxes, ha='center', va='bottom',
            bbox=dict(facecolor='white', alpha=0.7, boxstyle='round,pad=0.3'))
    
    ax.set_ylabel('Latency (μs)')
    ax.set_title(title)
    
    # Set sensible y-limits to prevent figure size issues
    if max(oblivious_filtered) / max(baseline_filtered) > 10:
        ax.set_yscale('log')
        ax.yaxis.set_major_formatter(ScalarFormatter())
        
        # Set reasonable y limits
        min_val = min(min(baseline_filtered) if len(baseline_filtered) > 0 else 0.1, 
                     min(oblivious_filtered) if len(oblivious_filtered) > 0 else 0.1)
        max_val = max(max(baseline_filtered) if len(baseline_filtered) > 0 else 1,
                     max(oblivious_filtered) if len(oblivious_filtered) > 0 else 1)
        
        # Ensure positive values for log scale
        min_val = max(0.1, min_val)
        
        ax.set_ylim(min_val * 0.5, max_val * 2)
    else:
        max_val = max(max(baseline_filtered) if len(baseline_filtered) > 0 else 0,
                     max(oblivious_filtered) if len(oblivious_filtered) > 0 else 0)
        ax.set_ylim(0, max_val * 1.2)
    
    fig.tight_layout()
    return fig

def plot_memory_usage(baseline_metrics, oblivious_metrics):
    """Create a bar chart comparing memory usage"""
    fig, ax = plt.subplots(figsize=(8, 6))
    
    labels = ['Baseline Router', 'Oblivious Router']
    values = [baseline_metrics['PeakMemoryUsageMB'], oblivious_metrics['PeakMemoryUsageMB']]
    
    bars = ax.bar(labels, values, color=['#3498db', '#e74c3c'])
    
    # Add text labels above bars
    for bar in bars:
        height = bar.get_height()
        ax.text(bar.get_x() + bar.get_width()/2., height * 1.01,
                f'{height:.2f} MB', ha='center', va='bottom')
    
    # Calculate ratio
    ratio = oblivious_metrics['PeakMemoryUsageMB'] / baseline_metrics['PeakMemoryUsageMB']
    ratio_text = f"Memory Ratio: {ratio:.2f}x"
    
    ax.text(0.5, 0.9, ratio_text, ha='center', va='center', transform=ax.transAxes,
           bbox=dict(facecolor='white', alpha=0.7, boxstyle='round,pad=0.3'))
    
    ax.set_ylabel('Peak Memory Usage (MB)')
    ax.set_title('Memory Overhead Comparison')
    ax.set_ylim(0, max(values) * 1.2)
    
    fig.tight_layout()
    return fig

def main():
    # Define the file paths
    baseline_filepath = 'baseline_router_metrics.csv'
    oblivious_filepath = 'oblivious_router_metrics.csv'
    
    # Create output directory if it doesn't exist
    output_dir = 'router_comparison_plots'
    os.makedirs(output_dir, exist_ok=True)
    
    # Load data from both files
    print("Loading data from CSV files...")
    baseline_data = load_metric_data(baseline_filepath)
    oblivious_data = load_metric_data(oblivious_filepath)
    
    # Extract aggregated metrics
    baseline_metrics = baseline_data['aggregated']
    oblivious_metrics = oblivious_data['aggregated']
    
    try:
        # 1. Plot throughput comparison
        print("Generating throughput comparison plot...")
        fig_throughput = plot_throughput_comparison(baseline_metrics, oblivious_metrics)
        fig_throughput.savefig(f'{output_dir}/throughput_comparison.png', dpi=300, bbox_inches='tight')
        plt.close(fig_throughput)  # Close figure to free memory
        
        # 2. Plot latency comparison (mean values)
        print("Generating mean latency comparison plot...")
        latency_metrics = ['InterestLatencyMean', 'DataLatencyMean', 'RetrievalLatencyMean']
        fig_latency = plot_bar_comparison(
            baseline_metrics, oblivious_metrics, 
            latency_metrics, 
            'Mean Latency Comparison', 
            'Latency (μs)',
            log_scale=True
        )
        fig_latency.savefig(f'{output_dir}/mean_latency_comparison.png', dpi=300, bbox_inches='tight')
        plt.close(fig_latency)  # Close figure to free memory
        
        # 3. Plot latency comparison (median values)
        print("Generating median latency comparison plot...")
        median_metrics = ['InterestLatencyMedian', 'DataLatencyMedian', 'RetrievalLatencyMedian']
        fig_median = plot_bar_comparison(
            baseline_metrics, oblivious_metrics, 
            median_metrics, 
            'Median Latency Comparison', 
            'Latency (μs)',
            log_scale=True
        )
        fig_median.savefig(f'{output_dir}/median_latency_comparison.png', dpi=300, bbox_inches='tight')
        plt.close(fig_median)  # Close figure to free memory
        
        # 4. Plot memory usage comparison
        print("Generating memory usage comparison plot...")
        fig_memory = plot_memory_usage(baseline_metrics, oblivious_metrics)
        fig_memory.savefig(f'{output_dir}/memory_usage_comparison.png', dpi=300, bbox_inches='tight')
        plt.close(fig_memory)  # Close figure to free memory
        
        # 5. Plot latency distributions
        latency_types = {
            'interest': 'Interest Packets', 
            'data': 'Data Packets', 
            'retrieval': 'Retrieval Operations'
        }
        
        for latency_type, label in latency_types.items():
            try:
                # Distribution plots
                print(f"Generating {latency_type} latency distribution plot...")
                fig_dist = plot_latency_distribution(
                    baseline_data['raw'][latency_type],
                    oblivious_data['raw'][latency_type],
                    latency_type,
                    f'{label} Latency Distribution'
                )
                fig_dist.savefig(f'{output_dir}/{latency_type}_latency_distribution.png', dpi=300, bbox_inches='tight')
                plt.close(fig_dist)  # Close figure to free memory
                
                # Box plots
                print(f"Generating {latency_type} latency box plot...")
                fig_box = plot_latency_boxplot(
                    baseline_data['raw'][latency_type],
                    oblivious_data['raw'][latency_type],
                    latency_type,
                    f'{label} Latency Box Plot'
                )
                fig_box.savefig(f'{output_dir}/{latency_type}_latency_boxplot.png', dpi=300, bbox_inches='tight')
                plt.close(fig_box)  # Close figure to free memory
            except Exception as e:
                print(f"Error generating {latency_type} plots: {e}")
                
        print(f"All plots have been saved to the '{output_dir}' directory.")
        
        # Print summary of key findings
        print("\nSummary of Key Findings:")
        
        throughput_ratio = baseline_metrics['Throughput'] / oblivious_metrics['Throughput']
        print(f"1. Throughput: Oblivious router is {throughput_ratio:.2f}x slower than baseline")
        
        for metric in latency_metrics:
            short_name = metric.replace('LatencyMean', '')
            ratio = oblivious_metrics[metric] / baseline_metrics[metric]
            print(f"2. {short_name} Latency: Oblivious router is {ratio:.2f}x slower than baseline")
        
        memory_ratio = oblivious_metrics['PeakMemoryUsageMB'] / baseline_metrics['PeakMemoryUsageMB']
        print(f"3. Memory Usage: Oblivious router uses {memory_ratio:.2f}x the memory of baseline")
            
    except Exception as e:
        print(f"Error generating plots: {e}")
        import traceback
        traceback.print_exc()

if __name__ == "__main__":
    main()