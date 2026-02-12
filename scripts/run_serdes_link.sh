#!/bin/bash
# SerDes Link Testbench Runner
# Usage: ./run_serdes_link.sh [scenario] [analyze]
#   scenario: basic (default), eye, s4p, long_ch
#   analyze:  if "yes", run Python EyeAnalyzer and DFE tap analysis after simulation

SCENARIO=${1:-basic}
ANALYZE=${2:-no}

echo "=========================================="
echo "SerDes Link Testbench"
echo "=========================================="
echo "Scenario: $SCENARIO"
echo ""

# Build if needed
echo "Building testbench..."
cd build
make serdes_link_tb -j4

if [ $? -ne 0 ]; then
    echo "Build failed!"
    exit 1
fi

# Run simulation
echo ""
echo "Running simulation..."
./tb/serdes_link_tb $SCENARIO

if [ $? -ne 0 ]; then
    echo "Simulation failed!"
    exit 1
fi

# Analyze with EyeAnalyzer and DFE tap plotter if requested
if [ "$ANALYZE" = "yes" ] || [ "$ANALYZE" = "y" ]; then
    echo ""
    echo "Running EyeAnalyzer..."
    cd ..
    python3 scripts/analyze_serdes_link.py $SCENARIO
    
    echo ""
    echo "Plotting DFE tap coefficients..."
    python3 scripts/plot_dfe_taps.py build/serdes_link_${SCENARIO}_dfe_taps.csv
fi

echo ""
echo "=========================================="
echo "Done!"
echo "=========================================="
