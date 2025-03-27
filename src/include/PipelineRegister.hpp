#pragma once
#include "Instruction.hpp"
#include <memory>
using namespace std;
struct PipelineRegister {
    bool valid = false;
    shared_ptr<Instruction> instruction = nullptr;
    uint32_t pc = 0;
    int aluResult = 0;
    int readData = 0;
    int rs1Value = 0;
    int rs2Value = 0;
    bool isBranch = false;
    bool branchTaken = false;
    uint32_t branchTarget = 0;
    
    void clear() {
        valid = false;
        instruction = nullptr;
        pc = 0;
        aluResult = 0;
        readData = 0;
        rs1Value = 0;
        rs2Value = 0;
        isBranch = false;
        branchTaken = false;
        branchTarget = 0;
    }
};
