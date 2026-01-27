#!/bin/bash
# Script to run channel S-parameter processing and validation
# Usage: ./run_channel_validation.sh [options]

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
OUTPUT_DIR="${PROJECT_ROOT}/build/channel_validation"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

print_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

print_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Check Python dependencies
check_dependencies() {
    print_info "Checking Python dependencies..."
    
    python3 -c "import numpy" 2>/dev/null || {
        print_error "numpy not installed. Install with: pip install numpy"
        exit 1
    }
    
    python3 -c "import scipy" 2>/dev/null || {
        print_error "scipy not installed. Install with: pip install scipy"
        exit 1
    }
    
    python3 -c "import matplotlib" 2>/dev/null || {
        print_warn "matplotlib not installed. Plots will be skipped."
    }
    
    print_info "Dependencies check passed."
}

# Run validation
run_validation() {
    print_info "Running channel validation..."
    
    mkdir -p "$OUTPUT_DIR"
    
    cd "$SCRIPT_DIR"
    python3 validate_channel.py -o "$OUTPUT_DIR" -v
    
    if [ $? -eq 0 ]; then
        print_info "Validation completed successfully!"
        print_info "Results saved to: $OUTPUT_DIR"
    else
        print_error "Validation failed!"
        exit 1
    fi
}

# Process S-parameter file
process_sparam() {
    local input_file="$1"
    local output_file="${2:-$OUTPUT_DIR/channel_config.json}"
    
    if [ ! -f "$input_file" ]; then
        print_error "Input file not found: $input_file"
        exit 1
    fi
    
    print_info "Processing S-parameter file: $input_file"
    
    cd "$SCRIPT_DIR"
    python3 process_sparam.py "$input_file" -o "$output_file" -m both -v
    
    if [ $? -eq 0 ]; then
        print_info "Processing completed!"
        print_info "Configuration saved to: $output_file"
    else
        print_error "Processing failed!"
        exit 1
    fi
}

# Show usage
show_usage() {
    echo "Usage: $0 [command] [options]"
    echo ""
    echo "Commands:"
    echo "  validate    Run full validation with synthetic data"
    echo "  process     Process an S-parameter file"
    echo "  help        Show this help message"
    echo ""
    echo "Examples:"
    echo "  $0 validate"
    echo "  $0 process channel.s2p -o config.json"
}

# Main
main() {
    case "${1:-validate}" in
        validate)
            check_dependencies
            run_validation
            ;;
        process)
            if [ -z "$2" ]; then
                print_error "Input file required for 'process' command"
                show_usage
                exit 1
            fi
            check_dependencies
            process_sparam "$2" "$3"
            ;;
        help|--help|-h)
            show_usage
            ;;
        *)
            print_error "Unknown command: $1"
            show_usage
            exit 1
            ;;
    esac
}

main "$@"
