#!/usr/bin/env gnuplot
# Plot all .perc files on one chart
# Usage: gnuplot plot_hdr_histogram.gp

set terminal pngcairo size 1000,600 enhanced font 'Verdana,10'
set output 'hdr-histogram.png'

set title 'Latency Distribution (HDR Histogram)'
set xlabel 'Latency (microseconds)'
set ylabel 'Percentile'
set logscale x
set grid
set key top left

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
    } else if (throughput_num >= 0.001) {
        label = sprintf("%.0fK TPS", throughput_num * 1000)
    } else {
        label = sprintf("%.3f TPS", throughput_num)
    }
    
    # Plot: column 2 (Value/latency) vs column 1 (Percentile)
    # Skip header lines
    plot_cmd = plot_cmd.sprintf("'%s' using 2:1 with lines title '%s'", file, label)
    file_index = file_index + 1
}

eval("plot ".plot_cmd)

