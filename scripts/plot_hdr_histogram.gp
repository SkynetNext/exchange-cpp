#!/usr/bin/env gnuplot
set terminal pngcairo size 1000,600 enhanced font 'Verdana,10'
set output 'hdr-histogram.png'

set title 'Latency Distribution (HDR Histogram)'
set xlabel 'Percentile'
set ylabel 'Latency (microseconds)'
set logscale y
set grid
set key bottom right

# X-axis: Percentile 0-100%, focus on tail (90%-99.999%)
set xrange [0:1]
set format x "%.0f%%"
set xtics (0, 0.9, 0.99, 0.999, 0.9999, 0.99999)

# Y-axis: Latency in microseconds, logarithmic scale
set yrange [0.1:1000]
set format y "%.0f"
set ytics (0.1, 1, 10, 100, 1000)

# Get sorted file list by throughput
file_list = system("ls -1 *.perc 2>/dev/null | sort -t- -k2 -n")
plot_cmd = ""
file_index = 1

do for [file in file_list] {
    if (file_index > 1) {
        plot_cmd = plot_cmd.", "
    }
    # Extract throughput from filename: {timestamp}-{perfMt}.perc
    throughput = system("echo '".file."' | sed 's/.*-//' | sed 's/\\.perc$//'")
    throughput_num = real(throughput)
    # Format label: 0.3 -> 300K, 2.0 -> 2.0M
    if (throughput_num >= 1.0) {
        label = sprintf("%.1fM", throughput_num)
    } else {
        label = sprintf("%.0fK", throughput_num * 1000)
    }
    # Plot: column 2 (Percentile) vs column 1 (Value in microseconds)
    # Skip first 2 lines (header)
    plot_cmd = plot_cmd.sprintf("'%s' using 2:1 every ::2 with lines lw 2 title '%s'", file, label)
    file_index = file_index + 1
}

eval("plot ".plot_cmd)
