#!/bin/bash
# Shell script to plot HDR Histogram .perc file to PNG using gnuplot
# Usage: plot_histogram.sh input.perc [output.png]

if [ $# -eq 0 ]; then
    echo "Usage: $0 input.perc [output.png]"
    echo ""
    echo "Example:"
    echo "  $0 1234567890-1.234.perc"
    echo "  $0 1234567890-1.234.perc histogram.png"
    exit 1
fi

INPUT_FILE="$1"
if [ -z "$2" ]; then
    OUTPUT_FILE="${INPUT_FILE%.perc}.png"
else
    OUTPUT_FILE="$2"
fi

# Check if gnuplot is available
if ! command -v gnuplot &> /dev/null; then
    echo "Error: gnuplot not found"
    echo "Please install gnuplot:"
    echo "  Ubuntu/Debian: sudo apt-get install gnuplot"
    echo "  macOS: brew install gnuplot"
    exit 1
fi

# Get script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Run gnuplot
gnuplot -e "input_file='$INPUT_FILE'; output_file='$OUTPUT_FILE'" "$SCRIPT_DIR/plot_hdr_histogram.gp"

if [ $? -eq 0 ]; then
    echo ""
    echo "Successfully generated: $OUTPUT_FILE"
else
    echo ""
    echo "Error: Failed to generate PNG file"
    exit 1
fi

