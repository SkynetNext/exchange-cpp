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
set key top left

# Set x-axis range to 0-1 (percentile as fraction)
set xrange [0:1]
set format x "%.2f"

# Get list of .perc files sorted by throughput (extract number from filename)
file_list = system("ls -1 *.perc 2>/dev/null | sort -t- -k2 -n")

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
    
    # Format label
    if (throughput_num >= 1.0) {
        label = sprintf("%.1fM TPS", throughput_num)
    } else {
        if (throughput_num >= 0.001) {
            label = sprintf("%.0fK TPS", throughput_num * 1000)
        } else {
            label = sprintf("%.3f TPS", throughput_num)
        }
    }
    
    # Plot: column 2 (Percentile 0.0-1.0) vs column 1 (Value/latency in microseconds)
    # Skip header lines (lines starting with "Value" or "---")
    # Format: Value Percentile TotalCount 1/(1-Percentile)
    plot_cmd = plot_cmd.sprintf("'%s' using 2:1 with lines title '%s'", file, label)
    file_index = file_index + 1
}

eval("plot ".plot_cmd)

