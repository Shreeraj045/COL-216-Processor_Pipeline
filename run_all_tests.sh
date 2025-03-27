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

# Find all test files automatically
TEST_FILES=$(find inputfiles -name "*.txt" -type f | sort)
DEFAULT_CYCLES=25  # Default number of cycles if not specified

echo -e "${YELLOW}Found $(echo "$TEST_FILES" | wc -l | tr -d ' ') test files to process.${NC}"
echo -e "${YELLOW}Running tests...${NC}"

# Run tests
for TEST_FILE in $TEST_FILES; do
    # Extract base filename without extension
    BASENAME=$(basename "$TEST_FILE" .txt)
    
    # Determine cycles to run (you could extract this from the file if needed)
    # For now use a default value, but this could be customized per test
    CYCLES=$DEFAULT_CYCLES
    
    echo -e "\n${YELLOW}Testing ${BASENAME}...${NC}"
    
    # Run non-forwarding processor
    echo "Running non-forwarding simulation for ${BASENAME}..."
    src/noforward "$TEST_FILE" $CYCLES > "outputfiles/actual/noforward_${BASENAME}.txt"
    
    # Run forwarding processor
    echo "Running forwarding simulation for ${BASENAME}..."
    src/forward "$TEST_FILE" $CYCLES > "outputfiles/actual/forward_${BASENAME}.txt"
    
    # Compare with expected outputs if they exist
    if [ -f "outputfiles/noforward_${BASENAME}.txt" ]; then
        echo -e "${YELLOW}Comparing non-forwarding results...${NC}"
        diff "outputfiles/noforward_${BASENAME}.txt" "outputfiles/actual/noforward_${BASENAME}.txt" > /dev/null
        if [ $? -eq 0 ]; then
            echo -e "${GREEN}✅ Non-forwarding test passed!${NC}"
        else
            echo -e "${RED}❌ Non-forwarding test failed. See differences:${NC}"
            diff "outputfiles/noforward_${BASENAME}.txt" "outputfiles/actual/noforward_${BASENAME}.txt" | head -n 10
        fi
    else
        echo -e "${YELLOW}No expected output file for non-forwarding test.${NC}"
    fi
    
    if [ -f "outputfiles/forward_${BASENAME}.txt" ]; then
        echo -e "${YELLOW}Comparing forwarding results...${NC}"
        diff "outputfiles/forward_${BASENAME}.txt" "outputfiles/actual/forward_${BASENAME}.txt" > /dev/null
        if [ $? -eq 0 ]; then
            echo -e "${GREEN}✅ Forwarding test passed!${NC}"
        else
            echo -e "${RED}❌ Forwarding test failed. See differences:${NC}"
            diff "outputfiles/forward_${BASENAME}.txt" "outputfiles/actual/forward_${BASENAME}.txt" | head -n 10
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
for TEST_FILE in $TEST_FILES; do
    BASENAME=$(basename "$TEST_FILE" .txt)
    STAGE_COUNT=$(grep -c ";" "outputfiles/actual/forward_${BASENAME}.txt")
    echo -e "${BASENAME}: ${STAGE_COUNT} pipeline stages"
done
