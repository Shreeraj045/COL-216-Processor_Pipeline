#pragma once
#include <vector>
#include <cstdint>
#include <string>
#include "Instruction.hpp"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <algorithm>
using namespace std;
class Memory {
private:
    vector<uint8_t> data;
    vector<Instruction> instructions;
    
public:
    Memory(size_t size = 1024*1024);  // Default 1MB memory
    
    // Memory access functions
    uint8_t readByte(uint32_t address) const;
    uint16_t readHalf(uint32_t address) const;
    uint32_t readWord(uint32_t address) const;
    
    void writeByte(uint32_t address, uint8_t value);
    void writeHalf(uint32_t address, uint16_t value);
    void writeWord(uint32_t address, uint32_t value);
    
    // Instruction memory functions
    void loadInstructions(const string& filename);
    Instruction getInstruction(uint32_t pc) const;
    size_t getInstructionCount() const { return instructions.size(); }
    
    // Reset memory to 0
    void reset();
};
