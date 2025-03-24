#include "../include/Instruction.hpp"

Instruction::Instruction() : machineCode(0), assembly("NOP") {
    decode();
}

Instruction::Instruction(uint32_t code, const std::string& asm_str) 
    : machineCode(code), assembly(asm_str) {
    decode();
}

void Instruction::decode() {
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
        // I-type immediate: sign-extended 12-bit value
        imm = (int32_t)(machineCode & 0xFFF00000) >> 20;
        // Sign extension
        if (imm & 0x800) {
            imm |= 0xFFFFF000;
        }
    } else if (isSType()) {
        // S-type immediate: sign-extended concatenation of bits
        imm = ((machineCode >> 25) & 0x7F) << 5;
        imm |= ((machineCode >> 7) & 0x1F);
        // Sign extension
        if (imm & 0x800) {
            imm |= 0xFFFFF000;
        }
    } else if (isBType()) {
        // B-type immediate: sign-extended 13-bit value
        imm = ((machineCode >> 31) & 0x1) << 12;
        imm |= ((machineCode >> 7) & 0x1) << 11;
        imm |= ((machineCode >> 25) & 0x3F) << 5;
        imm |= ((machineCode >> 8) & 0xF) << 1;
        // Sign extension
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
        // Sign extension
        if (imm & 0x100000) {
            imm |= 0xFFE00000;
        }
    }
}

bool Instruction::isRType() const {
    return (opcode == 0x33);  // Most ALU operations
}

bool Instruction::isIType() const {
    return (opcode == 0x13 || // Immediate ALU operations
            opcode == 0x03 || // Load operations
            opcode == 0x67);  // JALR
}

bool Instruction::isSType() const {
    return (opcode == 0x23);  // Store operations
}

bool Instruction::isBType() const {
    return (opcode == 0x63);  // Branch operations
}

bool Instruction::isUType() const {
    return (opcode == 0x37 || // LUI
            opcode == 0x17);  // AUIPC
}

bool Instruction::isJType() const {
    return (opcode == 0x6F);  // JAL
}

bool Instruction::isLoad() const {
    return (opcode == 0x03);  // Load operations
}

bool Instruction::isStore() const {
    return (opcode == 0x23);  // Store operations
}

bool Instruction::isBranch() const {
    return isBType();
}

bool Instruction::isJump() const {
    return (opcode == 0x6F || opcode == 0x67);  // JAL or JALR
}

bool Instruction::isALU() const {
    return (isRType() || opcode == 0x13);  // R-type or immediate ALU ops
}
