#include "../include/ForwardingProcessor.hpp"
#include <iostream>
#include <climits>

ForwardingProcessor::ForwardingProcessor() : Processor() {
}

void ForwardingProcessor::detectHazards() {
    // Initially assume no stall
    stall = false;
    
    // Check if we have a valid instruction in ID stage
    if (!ifId.valid) {
        return;
    }
    
    auto idInstr = ifId.instruction;
    
    // CASE 1: Load-use hazard - when we need a value that's being loaded from memory
    if (idEx.valid && idEx.instruction && idEx.instruction->isLoad()) {
        int loadDest = idEx.instruction->getRd();
        
        // Check if either source register of the instruction in ID matches the load destination
        if ((idInstr->getRs1() == loadDest && loadDest != 0) || 
            (idInstr->getRs2() == loadDest && loadDest != 0 && 
             !idInstr->isIType() && !idInstr->isUType() && !idInstr->isJType())) {
            // Load-use hazard detected, stall the pipeline
            stall = true;
            idEx.clear(); // Insert a bubble in ID/EX
            std::cout << "HAZARD: Load-use hazard detected, stalling" << std::endl;
            return;
        }
    }
    
    // We no longer need branch data hazard detection here since branches are evaluated in EX now
}

void ForwardingProcessor::stageID() {
    if (!ifId.valid) {
        idEx.clear();
        return;
    }
    
    // Check if we're stalled - if so, don't advance instruction from ID to EX
    if (stall) {
        idEx.clear(); // Insert a bubble in ID/EX
        return; // Keep instruction in IF/ID
    }
    
    // Copy the instruction and PC from IF/ID to ID/EX
    idEx.instruction = ifId.instruction;
    idEx.pc = ifId.pc;
    idEx.valid = true;
    
    // Read register values
    auto instr = idEx.instruction;
    int rs1 = instr->getRs1();
    int rs2 = instr->getRs2();
    int rs1Value = registers.read(rs1);
    int rs2Value = registers.read(rs2);
    
    // std::cout << "ID STAGE: Initial rs1(" << rs1 << ")=" << rs1Value 
    //           << ", rs2(" << rs2 << ")=" << rs2Value << std::endl;
    
    // Store values in pipeline register
    idEx.rs1Value = rs1Value;
    idEx.rs2Value = rs2Value;
    
    // Tag if this is a branch instruction - but don't evaluate it yet
    if (instr->isBranch() || instr->isJump()) {
        idEx.isBranch = true;
    } else {
        idEx.isBranch = false;
    }
    
    // Print debug information for the ID stage
    // auto cinstr = memory.getInstruction(idEx.pc);
    // std::string cinstrText = stripComments(cinstr.getAssembly());
    // std::cout << "************************************************" << std::endl;
    // std::cout << "Instruction at ID: " << cinstrText << " cycle " << cycleCount << std::endl;
    // std::cout << "************************************************" << std::endl;
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
    exMem.isBranch = idEx.isBranch;
    
    // Add debug info for jump instruction identification
    auto instr = exMem.instruction;
    // std::cout << "DEBUG: Instruction opcode: 0x" << std::hex << instr->getOpcode() << std::dec << std::endl;
    // std::cout << "DEBUG: isJType: " << instr->isJType() << ", isJump: " << instr->isJump() << std::endl;
    // std::cout << "DEBUG: Branch target would be: " << (idEx.pc + instr->getImm()) << std::endl;
    
    // Forward values from MEM/WB if needed
    int rs1 = idEx.instruction->getRs1();
    int rs2 = idEx.instruction->getRs2();
    
    // Fetch register values directly from register file instead of using cached values
    int rs1Value = registers.read(rs1);
    int rs2Value = registers.read(rs2);
    
    // // Add debug information about initial values
    // std::cout << "EX STAGE: Initial rs1(" << rs1 << ")=" << rs1Value 
    //           << ", rs2(" << rs2 << ")=" << rs2Value << std::endl;
    
    // Forward from MEM/WB first (older instruction)
    // This is critical for correct operation - we prioritize older instructions
    if (memWb.valid && memWb.instruction) {
        int memWbRd = memWb.instruction->getRd();
        
        // Check if MEM/WB is writing to a register we're reading from
        if (memWbRd != 0) {
            int wbValue = memWb.instruction->isLoad() ? memWb.readData : memWb.aluResult;
            
            if (rs1 == memWbRd) {
                rs1Value = wbValue;
                // std::cout << "EX STAGE: Forwarded MEM/WB to rs1, new value=" << rs1Value << std::endl;
            }
            if (rs2 == memWbRd) {
                rs2Value = wbValue;
                // std::cout << "EX STAGE: Forwarded MEM/WB to rs2, new value=" << rs2Value << std::endl;
            }
        }
    }
    
    // Then forward from EX/MEM (newer instruction) to override if needed
    // if (exMem.valid && exMem.instruction) {
    //     int exMemRd = exMem.instruction->getRd();
        
    //     // Check if EX/MEM is writing to a register we're reading from
    //     if (exMemRd != 0) {
    //         // Forward ALU result (not for loads)
    //         if (!exMem.instruction->isLoad()) {
    //             if (rs1 == exMemRd) {
    //                 rs1Value = exMem.aluResult;
    //                 std::cout << "EX STAGE: Forwarded EX/MEM to rs1, new value=" << rs1Value << std::endl;
    //             }
    //             if (rs2 == exMemRd) {
    //                 rs2Value = exMem.aluResult;
    //                 std::cout << "EX STAGE: Forwarded EX/MEM to rs2, new value=" << rs2Value << std::endl;
    //             }
    //         }
    //     }
    // }
    
    // Store the possibly forwarded values
    exMem.rs1Value = rs1Value;
    exMem.rs2Value = rs2Value;
    
    int aluResult = 0;
    
    // NEW BRANCH HANDLING: Evaluate branches in EX with forwarded values
    if (instr->isBranch()) {
        // Calculate branch target
        exMem.branchTarget = idEx.pc + instr->getImm();
        
        // Evaluate branch condition with forwarded values
        int funct3 = instr->getFunct3();
        
        switch (funct3) {
            case 0x0: // BEQ
                exMem.branchTaken = (rs1Value == rs2Value);
                std::cout << "EX STAGE: BEQ with forwarded rs1Val: " << rs1Value 
                         << " rs2Val: " << rs2Value 
                         << " taken: " << exMem.branchTaken << std::endl;
                break;
            case 0x1: // BNE
                exMem.branchTaken = (rs1Value != rs2Value);
                break;
            case 0x4: // BLT
                exMem.branchTaken = (rs1Value < rs2Value);
                break;
            case 0x5: // BGE
                exMem.branchTaken = (rs1Value >= rs2Value);
                break;
            case 0x6: // BLTU
                exMem.branchTaken = ((unsigned int)rs1Value < (unsigned int)rs2Value);
                break;
            case 0x7: // BGEU
                exMem.branchTaken = ((unsigned int)rs1Value >= (unsigned int)rs2Value);
                break;
            default:
                exMem.branchTaken = false;
                break;
        }
        
        // Handle branch decision immediately (after EX stage)
        if (exMem.branchTaken) {
            // Branch is taken, flush pipeline and redirect
            ifId.clear();
            idEx.clear();
            
            // Set new PC for next IF
            pc = exMem.branchTarget;
            
            // std::cout << "EX STAGE: Branch taken! Target PC: " << exMem.branchTarget << std::endl;
        }
    } else if (instr->isJump() || instr->getOpcode() == 0x6F) { // Added direct opcode check
        // Modified condition to catch all jump instructions
        // std::cout << "DEBUG: Jump instruction detected!" << std::endl;
        
        // For jumps
        if (instr->getOpcode() == 0x6F) { // JAL
            exMem.branchTarget = idEx.pc + instr->getImm();
            aluResult = idEx.pc + 4; // Return address
            // std::cout << "DEBUG: JAL jump to " << exMem.branchTarget << std::endl;
        } else if (instr->getOpcode() == 0x67) { // JALR
            exMem.branchTarget = (rs1Value + instr->getImm()) & ~1; // Clear LSB
            aluResult = idEx.pc + 4; // Return address
            // std::cout << "DEBUG: JALR jump to " << exMem.branchTarget << std::endl;
        }
        
        // Jumps are always taken
        exMem.branchTaken = true;
        
        // Flush and redirect
        ifId.clear();
        idEx.clear();
        pc = exMem.branchTarget;
        
        // std::cout << "EX STAGE: Jump taken! Target PC: " << exMem.branchTarget << std::endl;
    } else {
        // Regular ALU operations - same as before
        if (instr->isRType()) {
            int funct3 = instr->getFunct3();
            int funct7 = instr->getFunct7();
            
            // Check if this is an M-extension instruction
            if (funct7 == 0x01) {
                // M-extension instructions (MUL/DIV/REM)
                switch (funct3) {
                    case 0x0: // MUL
                        aluResult = rs1Value * rs2Value;
                        break;
                    case 0x1: // MULH
                        // Signed * Signed -> High bits
                        {
                            int64_t a = static_cast<int64_t>(rs1Value);
                            int64_t b = static_cast<int64_t>(rs2Value);
                            int64_t result = a * b;
                            aluResult = static_cast<int>(result >> 32);
                        }
                        break;
                    case 0x2: // MULHSU
                        // Signed * Unsigned -> High bits
                        {
                            int64_t a = static_cast<int64_t>(rs1Value);
                            uint64_t b = static_cast<uint64_t>(static_cast<uint32_t>(rs2Value));
                            int64_t result = a * b;
                            aluResult = static_cast<int>(result >> 32);
                        }
                        break;
                    case 0x3: // MULHU
                        // Unsigned * Unsigned -> High bits
                        {
                            uint64_t a = static_cast<uint64_t>(static_cast<uint32_t>(rs1Value));
                            uint64_t b = static_cast<uint64_t>(static_cast<uint32_t>(rs2Value));
                            uint64_t result = a * b;
                            aluResult = static_cast<int>(result >> 32);
                        }
                        break;
                    case 0x4: // DIV
                        // Check for division by zero
                        if (rs2Value == 0) {
                            aluResult = -1; // As per spec: division by zero returns -1
                        } 
                        // Check for overflow condition (INT_MIN / -1)
                        else if (rs1Value == INT_MIN && rs2Value == -1) {
                            aluResult = INT_MIN; // Return INT_MIN as specified
                        } 
                        else {
                            aluResult = rs1Value / rs2Value;
                        }
                        break;
                    case 0x5: // DIVU
                        // Unsigned division
                        if (rs2Value == 0) {
                            aluResult = 0xFFFFFFFF; // Max unsigned value for division by zero
                        } else {
                            aluResult = static_cast<int>((static_cast<uint32_t>(rs1Value) / 
                                                         static_cast<uint32_t>(rs2Value)));
                        }
                        break;
                    case 0x6: // REM
                        // Remainder of signed division
                        if (rs2Value == 0) {
                            aluResult = rs1Value; // Remainder of x/0 is x
                        } 
                        // Handle overflow case (INT_MIN % -1)
                        else if (rs1Value == INT_MIN && rs2Value == -1) {
                            aluResult = 0; // Remainder is 0 in this case
                        } 
                        else {
                            aluResult = rs1Value % rs2Value;
                        }
                        break;
                    case 0x7: // REMU
                        // Remainder of unsigned division
                        if (rs2Value == 0) {
                            aluResult = rs1Value; // Remainder of x/0 is x
                        } else {
                            aluResult = static_cast<int>((static_cast<uint32_t>(rs1Value) % 
                                                         static_cast<uint32_t>(rs2Value)));
                        }
                        break;
                }
            } else {
                // Regular R-type instructions
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
    }
    
    exMem.aluResult = aluResult;
    //if branch and brach taken set btpc as new pc ; 
    if(exMem.isBranch && exMem.branchTaken){
        btpc = exMem.branchTarget;
        tibt = true;
    }

    // Debug output
    auto kinstr = memory.getInstruction(idEx.pc);
    // std::cout << "************************************************" << std::endl;
    // std::cout << "Instruction at EX: " << stripComments(kinstr.getAssembly()) << " cycle " << cycleCount << std::endl;
    // std::cout << "************************************************" << std::endl;
}
