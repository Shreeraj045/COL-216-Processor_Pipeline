#include "../include/Instruction.hpp"
using namespace std;

Instruction::Instruction() : machineCode(0), assembly("NOP") {
    decode();
}

Instruction::Instruction(uint32_t code, const string& asm_str) : machineCode(code), assembly(asm_str) {
    decode();
}

void Instruction::decode() {
    // get different vals from machine code
    // lower 7 bits 1 in 0x7F, 5 bits 1 for 0x1F
    opcode = machineCode & 0x7F;
    rd = (machineCode >> 7) & 0x1F;
    funct3 = (machineCode >> 12) & 0x7;
    rs1 = (machineCode >> 15) & 0x1F;
    rs2 = (machineCode >> 20) & 0x1F;
    funct7 = (machineCode >> 25) & 0x7F;

    
    // Decode immediate based on instruction format
    if (isRType()) {
        imm = 0; // No immediate for R-type
    } else if (isIType()) {
        imm = (int32_t)(machineCode & 0xFFF00000) >> 20;
        // Sign extension
        if (imm & 0x800) {
            imm |= 0xFFFFF000;
        }
    } else if (isSType()) {
        imm = ((machineCode >> 25) & 0x7F) << 5;
        imm |= ((machineCode >> 7) & 0x1F);
        if (imm & 0x800) {
            imm |= 0xFFFFF000;
        }
    } else if (isBType()) {
        // B-type immediate: sign-extended 13-bit value
        imm = ((machineCode >> 31) & 0x1) << 12;
        imm |= ((machineCode >> 7) & 0x1) << 11;
        imm |= ((machineCode >> 25) & 0x3F) << 5;
        imm |= ((machineCode >> 8) & 0xF) << 1;
        if (imm & 0x1000) {
            imm |= 0xFFFFE000;
        }
    } else if (isUType()) {
        // U-type immediate: upper 20 bits
        imm = (machineCode & 0xFFFFF000);
    } else if (isJType()) {
        // J-type immediate: sign-extended 21-bit value
        imm = ((machineCode >> 31) & 0x1) << 20;
        imm |= ((machineCode >> 12) & 0xFF) << 12;
        imm |= ((machineCode >> 20) & 0x1) << 11;
        imm |= ((machineCode >> 21) & 0x3FF) << 1;
        if (imm & 0x100000) {
            imm |= 0xFFE00000;
        }
    }
}

bool Instruction::isRType() const {
    return (opcode == 0x33);  // Most ALU operations including M-extension
}

// Add a helper method to identify M-extension instructions
bool Instruction::isRV32M() const {
    return isRType() && (funct7 == 0x01);  // M-extension has funct7 = 0x01
}

bool Instruction::isIType() const {
    // Immediate ALU, Load, JALR operations
    return (opcode == 0x13 || opcode == 0x03 || opcode == 0x67);  
}

bool Instruction::isSType() const {
    // Store operations
    return (opcode == 0x23);  
}

bool Instruction::isBType() const {
    // Branch operations
    return (opcode == 0x63);
}

bool Instruction::isUType() const {
    // lui, auipc
    return (opcode == 0x37 || opcode == 0x17); 
}

bool Instruction::isJType() const {
    // Jal
    return (opcode == 0x6F);  
}

bool Instruction::isLoad() const {
    // Load operations
    return (opcode == 0x03);  
}

bool Instruction::isJump() const {
    return (opcode == 0x6F || opcode == 0x67);  // JAL or JALR
}

bool Instruction::isALU() const {
    return (isRType() || opcode == 0x13);  // R-type or immediate ALU ops
}
