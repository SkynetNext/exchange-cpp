#!/usr/bin/env gnuplot
# Plot all .perc files on one chart
# Usage: gnuplot plot_hdr_histogram.gp

set terminal pngcairo size 1000,600 enhanced font 'Verdana,10'
set output 'hdr-histogram.png'

set title 'Latency Distribution (HDR Histogram)'
set xlabel 'Percentile'
set ylabel 'Latency (microseconds)'
set logscale y
set grid
set key bottom right

# Set y-axis range and format
set yrange [0.1:1000]
set format y "%.0f"
set ytics (0.1, 1, 10, 100, 1000)

# Set x-axis range: percentile is stored as 0.0-1.0, display as 0-100%
set xrange [0:1]
set format x "%.0f%%"
# Set major ticks at key percentiles: 0%, 90%, 99%, 99.9%, 99.99%, 99.999%
set xtics (0, 0.9, 0.99, 0.999, 0.9999, 0.99999)
set mxtics 5

# Get list of .perc files sorted by throughput (extract number from filename)
# Sort by throughput value (second field after '-')
file_list = system("ls -1 *.perc 2>/dev/null | sort -t- -k2 -n")

# Define color palette matching reference image style
set style line 1 lc rgb '#1f77b4' lw 2  # blue
set style line 2 lc rgb '#ff7f0e' lw 2  # orange  
set style line 3 lc rgb '#2ca02c' lw 2  # green
set style line 4 lc rgb '#d62728' lw 2  # red
set style line 5 lc rgb '#9467bd' lw 2  # purple
set style line 6 lc rgb '#8c564b' lw 2  # brown
set style line 7 lc rgb '#e377c2' lw 2  # pink
set style line 8 lc rgb '#7f7f7f' lw 2  # gray
set style line 9 lc rgb '#bcbd22' lw 2  # yellow-green
set style line 10 lc rgb '#17becf' lw 2 # cyan
set style line 11 lc rgb '#aec7e8' lw 2 # light blue

# Build plot command
plot_cmd = ""
file_index = 1

do for [file in file_list] {
    if (file_index > 1) {
        plot_cmd = plot_cmd.", "
    }
    
    # Extract throughput from filename: {timestamp}-{perfMt}.perc
    throughput = system("echo '".file."' | sed 's/.*-//' | sed 's/\\.perc$//'")
    throughput_num = real(throughput)
    
    # Format label to match reference image style
    if (throughput_num >= 1.0) {
        label = sprintf("%.1fM", throughput_num)
    } else {
        if (throughput_num >= 0.001) {
            label = sprintf("%.0fK", throughput_num * 1000)
        } else {
            label = sprintf("%.3f", throughput_num)
        }
    }
    
    # Assign color based on file index (cycle through colors)
    color_idx = ((file_index - 1) % 11) + 1
    
    # Plot: column 2 (Percentile 0.0-1.0) vs column 1 (Value/latency in microseconds)
    # Skip header lines (lines starting with "Value" or "---")
    # Format: Value Percentile TotalCount 1/(1-Percentile)
    # Note: Data is already in microseconds, no need to multiply
    # Skip first 2 lines (header)
    plot_cmd = plot_cmd.sprintf("'%s' using 2:1 every ::2 with lines ls %d title '%s'", file, color_idx, label)
    file_index = file_index + 1
}

eval("plot ".plot_cmd)

