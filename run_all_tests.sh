#!/bin/bash

# Colors for output
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[0;33m'
NC='\033[0m' # No Color

echo -e "${YELLOW}===== RISC-V Processor Simulator Tester =====${NC}"

# Create necessary directories
mkdir -p outputfiles/actual

# Build the simulator
echo -e "${YELLOW}Building simulator...${NC}"
cd src
make clean
make
if [ $? -ne 0 ]; then
    echo -e "${RED}Build failed! Aborting tests.${NC}"
    exit 1
fi
echo -e "${GREEN}Build completed successfully.${NC}"
cd ..

# List of test cases
TESTS=("basic_alu" "data_dep" "load_use" "branch_jump" "strlen")
CYCLES=(20 20 20 20 30)

echo -e "${YELLOW}Running tests...${NC}"

# Run tests
for i in "${!TESTS[@]}"; do
    TEST=${TESTS[$i]}
    CYCLE=${CYCLES[$i]}
    
    echo -e "\n${YELLOW}Testing ${TEST}...${NC}"
    
    # Run non-forwarding processor
    echo "Running non-forwarding simulation for ${TEST}..."
    src/noforward inputfiles/${TEST}.txt $CYCLE > outputfiles/actual/noforward_${TEST}.txt
    
    # Run forwarding processor
    echo "Running forwarding simulation for ${TEST}..."
    src/forward inputfiles/${TEST}.txt $CYCLE > outputfiles/actual/forward_${TEST}.txt
    
    # Compare with expected outputs if they exist
    if [ -f outputfiles/noforward_${TEST}.txt ]; then
        echo -e "${YELLOW}Comparing non-forwarding results...${NC}"
        diff outputfiles/noforward_${TEST}.txt outputfiles/actual/noforward_${TEST}.txt > /dev/null
        if [ $? -eq 0 ]; then
            echo -e "${GREEN}✅ Non-forwarding test passed!${NC}"
        else
            echo -e "${RED}❌ Non-forwarding test failed. See differences:${NC}"
            diff outputfiles/noforward_${TEST}.txt outputfiles/actual/noforward_${TEST}.txt | head -n 10
        fi
    else
        echo -e "${YELLOW}No expected output file for non-forwarding test.${NC}"
    fi
    
    if [ -f outputfiles/forward_${TEST}.txt ]; then
        echo -e "${YELLOW}Comparing forwarding results...${NC}"
        diff outputfiles/forward_${TEST}.txt outputfiles/actual/forward_${TEST}.txt > /dev/null
        if [ $? -eq 0 ]; then
            echo -e "${GREEN}✅ Forwarding test passed!${NC}"
        else
            echo -e "${RED}❌ Forwarding test failed. See differences:${NC}"
            diff outputfiles/forward_${TEST}.txt outputfiles/actual/forward_${TEST}.txt | head -n 10
        fi
    else
        echo -e "${YELLOW}No expected output file for forwarding test.${NC}"
    fi
done

echo -e "\n${GREEN}All tests have been executed!${NC}"
echo -e "${YELLOW}Output files are available in outputfiles/actual/${NC}"

# Create a summary
echo -e "\n${YELLOW}==== Test Summary ====${NC}"
echo "Instructions executed for each test:"
for TEST in "${TESTS[@]}"; do
    echo -e "${TEST}: $(grep -c ";" outputfiles/actual/forward_${TEST}.txt) pipeline stages"
done
