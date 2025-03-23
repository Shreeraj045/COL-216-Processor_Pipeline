#include "../include/ForwardingProcessor.hpp"

ForwardingProcessor::ForwardingProcessor() : Processor() {
}

void ForwardingProcessor::detectHazards() {
    // In forwarding processor, we only stall on load-use hazards
    stall = false;
    
    // Check if we have a valid instruction in ID stage
    if (!ifId.valid || !idEx.valid) {
        return;
    }
    
    auto idInstr = ifId.instruction;
    
    // Check if the instruction in ID needs a register value that is being loaded
    if (idEx.instruction->isLoad()) {
        int loadDest = idEx.instruction->getRd();
        
        // Check if either source register of the instruction in ID matches the load destination
        if ((idInstr->getRs1() == loadDest && loadDest != 0) || 
            (idInstr->getRs2() == loadDest && loadDest != 0 && 
             !idInstr->isIType() && !idInstr->isUType() && !idInstr->isJType())) {
            // Load-use hazard detected, stall the pipeline
            stall = true;
            
            // Keep IF/ID register unchanged
            // Insert a bubble in ID/EX
            idEx.clear();
        }
    }
}

void ForwardingProcessor::stageEX() {
    if (!idEx.valid) {
        exMem.clear();
        return;
    }
    
    // Copy values from ID/EX to EX/MEM
    exMem.instruction = idEx.instruction;
    exMem.pc = idEx.pc;
    exMem.valid = true;
    
    // Forward values from MEM/WB if needed
    int rs1 = idEx.instruction->getRs1();
    int rs2 = idEx.instruction->getRs2();
    
    // Initialize with values from registers
    int rs1Value = idEx.rs1Value;
    int rs2Value = idEx.rs2Value;
    
    // Forward from EX/MEM
    if (exMem.valid) {
        int exMemRd = exMem.instruction->getRd();
        
        // Check if EX/MEM is writing to a register we're reading from
        if (exMemRd != 0) {
            // Forward ALU result (not for loads)
            if (!exMem.instruction->isLoad()) {
                if (rs1 == exMemRd) {
                    rs1Value = exMem.aluResult;
                }
                if (rs2 == exMemRd) {
                    rs2Value = exMem.aluResult;
                }
            }
        }
    }
    
    // Forward from MEM/WB
    if (memWb.valid) {
        int memWbRd = memWb.instruction->getRd();
        
        // Check if MEM/WB is writing to a register we're reading from
        if (memWbRd != 0) {
            int wbValue = memWb.instruction->isLoad() ? memWb.readData : memWb.aluResult;
            
            // Forward only if not already forwarded from EX/MEM
            if (rs1 == memWbRd && (exMem.instruction->getRd() != rs1 || exMem.instruction->getRd() == 0)) {
                rs1Value = wbValue;
            }
            if (rs2 == memWbRd && (exMem.instruction->getRd() != rs2 || exMem.instruction->getRd() == 0)) {
                rs2Value = wbValue;
            }
        }
    }
    
    // Store the possibly forwarded values
    exMem.rs1Value = rs1Value;
    exMem.rs2Value = rs2Value;
    
    // Execute ALU operation with possibly forwarded values
    auto instr = exMem.instruction;
    int aluResult = 0;
    
    if (instr->isRType()) {
        int funct3 = instr->getFunct3();
        int funct7 = instr->getFunct7();
        
        switch (funct3) {
            case 0x0: // ADD/SUB
                if (funct7 == 0x00)
                    aluResult = rs1Value + rs2Value; // ADD
                else if (funct7 == 0x20)
                    aluResult = rs1Value - rs2Value; // SUB
                break;
            case 0x1: // SLL
                aluResult = rs1Value << (rs2Value & 0x1F);
                break;
            case 0x2: // SLT
                aluResult = (rs1Value < rs2Value) ? 1 : 0;
                break;
            case 0x3: // SLTU
                aluResult = ((unsigned int)rs1Value < (unsigned int)rs2Value) ? 1 : 0;
                break;
            case 0x4: // XOR
                aluResult = rs1Value ^ rs2Value;
                break;
            case 0x5: // SRL/SRA
                if (funct7 == 0x00)
                    aluResult = (unsigned int)rs1Value >> (rs2Value & 0x1F); // SRL
                else if (funct7 == 0x20)
                    aluResult = rs1Value >> (rs2Value & 0x1F); // SRA
                break;
            case 0x6: // OR
                aluResult = rs1Value | rs2Value;
                break;
            case 0x7: // AND
                aluResult = rs1Value & rs2Value;
                break;
        }
    } else if (instr->isIType()) {
        int funct3 = instr->getFunct3();
        int imm = instr->getImm();
        
        if (instr->getOpcode() == 0x13) { // ALU with immediate
            switch (funct3) {
                case 0x0: // ADDI
                    aluResult = rs1Value + imm;
                    break;
                case 0x2: // SLTI
                    aluResult = (rs1Value < imm) ? 1 : 0;
                    break;
                case 0x3: // SLTIU
                    aluResult = ((unsigned int)rs1Value < (unsigned int)imm) ? 1 : 0;
                    break;
                case 0x4: // XORI
                    aluResult = rs1Value ^ imm;
                    break;
                case 0x6: // ORI
                    aluResult = rs1Value | imm;
                    break;
                case 0x7: // ANDI
                    aluResult = rs1Value & imm;
                    break;
                case 0x1: // SLLI
                    aluResult = rs1Value << (imm & 0x1F);
                    break;
                case 0x5: // SRLI/SRAI
                    if ((imm >> 5) == 0)
                        aluResult = (unsigned int)rs1Value >> (imm & 0x1F); // SRLI
                    else
                        aluResult = rs1Value >> (imm & 0x1F); // SRAI
                    break;
            }
        } else if (instr->getOpcode() == 0x03) { // Load
            // Calculate memory address
            aluResult = rs1Value + imm;
        } else if (instr->getOpcode() == 0x67) { // JALR
            // Store return address (PC+4)
            aluResult = idEx.pc + 4;
        }
    } else if (instr->isSType()) { // Store
        // Calculate memory address
        aluResult = rs1Value + instr->getImm();
    } else if (instr->isBType()) { // Branch
        // ALU result not used for branches
    } else if (instr->isUType()) {
        if (instr->getOpcode() == 0x37) { // LUI
            aluResult = instr->getImm();
        } else if (instr->getOpcode() == 0x17) { // AUIPC
            aluResult = idEx.pc + instr->getImm();
        }
    } else if (instr->isJType()) { // JAL
        // Store return address (PC+4)
        aluResult = idEx.pc + 4;
    }
    
    exMem.aluResult = aluResult;
}
