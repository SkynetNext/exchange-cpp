#!/usr/bin/env gnuplot
# Gnuplot script to convert HDR Histogram .perc file to PNG
# Usage: gnuplot -e "input_file='your-file.perc'; output_file='output.png'" plot_hdr_histogram.gp
# Or: gnuplot plot_hdr_histogram.gp your-file.perc output.png

# Get input and output filenames from command line or environment
if (exists("ARG1")) {
    input_file = ARG1
} else if (!exists("input_file")) {
    print "Error: Please provide input file as ARG1 or set input_file variable"
    exit
}

if (exists("ARG2")) {
    output_file = ARG2
} else if (!exists("output_file")) {
    # Default output filename: replace .perc with .png
    output_file = system("echo '".input_file."' | sed 's/\\.perc$/.png/'")
}

# Set terminal to PNG with high quality
set terminal pngcairo size 1000,600 enhanced font 'Verdana,10'
set output output_file

# Set title and labels
set title 'Latency Distribution (HDR Histogram)'
set xlabel 'Latency (microseconds)'
set ylabel 'Percentile'
set grid

# Use logarithmic scale for x-axis (latency)
set logscale x

# Set format for x-axis (show microseconds)
set format x "%.1f"

# Set format for y-axis (show as percentage)
set format y "%.2f"

# Set key position
set key top left

# Plot the percentile distribution
# Column 1: Percentile (0.0-1.0)
# Column 2: Value (latency in microseconds)
# Skip header lines (lines starting with "Value" or "---")
plot input_file using 2:1 with lines title 'Percentile Distribution' lw 2

# Print confirmation
print "Generated PNG: ".output_file

