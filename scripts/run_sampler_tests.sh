#!/bin/bash

# run_sampler_tests.sh
# Script to run all Sampler module unit tests independently
#
# Each test is run in a separate process to avoid SystemC simulator state conflicts

# Color codes for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Test executables and descriptions (compatible with bash 3.2)
TEST_NAMES=(
    "test_sampler_basic_decision"
    "test_sampler_de_negative_input"
    "test_sampler_de_output"
    "test_sampler_fuzzy_decision"
    "test_sampler_hysteresis_behavior"
    "test_sampler_negative_decision"
    "test_sampler_noise_effect"
    "test_sampler_offset_effect"
    "test_sampler_output_range_neg05"
    "test_sampler_output_range_neg01"
    "test_sampler_output_range_zero"
    "test_sampler_output_range_pos01"
    "test_sampler_output_range_pos05"
    "test_sampler_validation_params"
    "test_sampler_validation_phase_source"
    "test_sampler_validation_valid_phase_source"
)

TEST_DESCS=(
    "Basic Decision Tests"
    "DE Negative Input Tests"
    "DE Output Tests"
    "Fuzzy Decision Tests"
    "Hysteresis Behavior Tests"
    "Negative Decision Tests"
    "Noise Effect Tests"
    "Offset Effect Tests"
    "Output Range -0.5V Tests"
    "Output Range -0.1V Tests"
    "Output Range 0.0V Tests"
    "Output Range +0.1V Tests"
    "Output Range +0.5V Tests"
    "Validation Parameters Tests"
    "Validation Phase Source Tests"
    "Validation Valid Phase Source Tests"
)

# Find test executables directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/../build/tests"

# Check if build directory exists
if [[ ! -d "$BUILD_DIR" ]]; then
    echo -e "${RED}Error: Build directory not found: $BUILD_DIR${NC}"
    echo -e "${YELLOW}Please build the tests first:${NC}"
    echo "  cd ${SCRIPT_DIR}/../build/tests && make"
    exit 1
fi

# Change to build directory
cd "$BUILD_DIR" || exit 1

# Statistics
total_tests=0
passed_tests=0
failed_tests=0
declare -a failed_test_names

echo ""
echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}  Sampler Module Unit Tests${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

# Run each test
for i in "${!TEST_NAMES[@]}"; do
    test_exec="${TEST_NAMES[$i]}"
    desc="${TEST_DESCS[$i]}"
    
    # Check if executable exists
    if [[ ! -x "./$test_exec" ]]; then
        echo -e "${YELLOW}Warning: $test_exec not found, skipping...${NC}"
        continue
    fi
    
    echo -e "${BLUE}========================================${NC}"
    echo -e "${BLUE}Test Suite: ${desc}${NC}"
    echo -e "${BLUE}Executable: ${test_exec}${NC}"
    echo -e "${BLUE}========================================${NC}"
    
    # Get list of individual test cases
    test_list=$(./"$test_exec" --gtest_list_tests 2>&1 | grep -v "^[[:space:]]*$")
    
    # Parse test suite and test case names
    current_suite=""
    suite_passed=0
    suite_failed=0
    
    while IFS= read -r line; do
        # Check if this is a test suite line (ends with .)
        if [[ "$line" =~ ^([A-Za-z0-9_]+)\.$ ]]; then
            current_suite="${BASH_REMATCH[1]}"
        # Check if this is a test case line (starts with spaces)
        elif [[ "$line" =~ ^[[:space:]]+([A-Za-z0-9_]+) ]]; then
            test_case="${BASH_REMATCH[1]}"
            test_filter="${current_suite}.${test_case}"
            
            echo -e "${BLUE}Running: ${test_filter}${NC}"
            
            # Run individual test in separate process
            test_output=$(./"$test_exec" --gtest_filter="${test_filter}" 2>&1)
            test_exit=$?
            
            # Count this test
            total_tests=$((total_tests + 1))
            
            # Check result
            if [[ $test_exit -eq 0 ]]; then
                passed_tests=$((passed_tests + 1))
                suite_passed=$((suite_passed + 1))
                echo -e "${GREEN}  ✓ ${test_filter} PASSED${NC}"
            else
                failed_tests=$((failed_tests + 1))
                suite_failed=$((suite_failed + 1))
                failed_test_names+=("${test_filter}")
                echo -e "${RED}  ✗ ${test_filter} FAILED${NC}"
                # Show error details
                echo "$test_output" | grep -A 5 "\[  FAILED  \]\|Failure\|Error"
            fi
        fi
    done <<< "$test_list"
    
    echo ""
    echo -e "${BLUE}Suite Summary: ${GREEN}${suite_passed} passed${NC}, ${RED}${suite_failed} failed${NC}"
    echo ""
done

# Print summary
echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}  Test Summary${NC}"
echo -e "${BLUE}========================================${NC}"
echo -e "Total tests run:    ${BLUE}${total_tests}${NC}"
echo -e "Tests passed:       ${GREEN}${passed_tests}${NC}"
echo -e "Tests failed:       ${RED}${failed_tests}${NC}"

if [[ $failed_tests -gt 0 ]]; then
    echo ""
    echo -e "${RED}Failed test suites:${NC}"
    for failed_test in "${failed_test_names[@]}"; do
        echo -e "  ${RED}✗ ${failed_test}${NC}"
    done
    echo ""
    exit 1
else
    echo ""
    echo -e "${GREEN}========================================${NC}"
    echo -e "${GREEN}  All tests PASSED! ✓${NC}"
    echo -e "${GREEN}========================================${NC}"
    echo ""
    exit 0
fi
