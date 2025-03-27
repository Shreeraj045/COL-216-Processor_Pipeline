#pragma once
#include <string>
#include <cstdint>
#include <iostream>
using namespace std;
class Instruction {
private:
    uint32_t machineCode;
    string assembly;
    
    // Decoded fields
    int opcode;
    int rd;
    int rs1;
    int rs2;
    int funct3;
    int funct7;
    int imm;
    
public:
    Instruction();
    Instruction(uint32_t machineCode, const string& assembly = "");
    
    // Getters
    uint32_t getMachineCode() const { return machineCode; }
    string getAssembly() const { return assembly; }
    int getOpcode() const { return opcode; }
    int getRd() const { return rd; }
    int getRs1() const { return rs1; }
    int getRs2() const { return rs2; }
    int getFunct3() const { return funct3; }
    int getFunct7() const { return funct7; }
    int getImm() const { return imm; }
    
    // Decode the instruction fields
    void decode();
    
    // Instruction type identification
    bool isRType() const;
    bool isIType() const;
    bool isSType() const;
    bool isBType() const;
    bool isUType() const;
    bool isJType() const;
    
    // Check if instruction is from RV32M extension
    bool isRV32M() const;
    
    // Specific instruction identification
    bool isLoad() const;
    bool isJump() const;
    bool isALU() const;
};
