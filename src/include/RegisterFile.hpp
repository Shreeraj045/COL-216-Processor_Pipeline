#pragma once
#include <array>

class RegisterFile {
private:
    std::array<int, 32> registers;
    
public:
    RegisterFile();
    
    // Get register value, x0 always returns 0
    int read(int regNum) const;
    
    // Set register value, writes to x0 are ignored
    void write(int regNum, int value);
    
    // Reset all registers to 0
    void reset();
};
