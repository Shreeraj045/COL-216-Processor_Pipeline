#include "../include/Processor.hpp"
#include <iostream>
#include <iomanip>
#include <algorithm>

Processor::Processor() : pc(0), cycleCount(0), instructionCount(0), stall(false) {
}

void Processor::loadProgram(const std::string& filename) {
    reset();
    memory.loadInstructions(filename);
}

void Processor::run(int cycles) {
    // For the very first instruction, record its IF stage before executing any pipeline stage
    if (cycles > 0) {
        // Only if we're just starting execution (cycleCount == 0)
        if (cycleCount == 0) {
            auto firstInstr = memory.getInstruction(pc);
            std::string instrText = stripComments(firstInstr.getAssembly());
            updateOrAddInstruction(instrText, "IF");
        }
    }

    for (int i = 0; i < cycles; ++i) {
        // Execute pipeline stages in reverse order to avoid overwriting
        stageWB();
        stageMEM();
        stageEX();
        
        // Detect hazards BEFORE ID and IF stages
        detectHazards();
        
        // Now execute ID and IF which will respect the stall flag
        stageID();
        stageIF();
        
        // Update the pipeline table with current state for the NEXT cycle
        cycleCount++;
        updatePipelineTable();
    }
    
    // Only print the pipeline diagram at the end
    printPipelineDiagram();
}

void Processor::reset() {
    pc = 0;
    cycleCount = 0;
    instructionCount = 0;
    stall = false;
    
    registers.reset();
    memory.reset();
    
    ifId.clear();
    idEx.clear();
    exMem.clear();
    memWb.clear();
    
    // Clear pipeline table
    pipelineTable.clear();
}

void Processor::stageIF() {
    if (stall) {
        return; // Don't fetch new instruction if stalled
    }
    
    // Fetch the instruction at the current PC
    auto instr = memory.getInstruction(pc);
    
    // Update IF/ID pipeline register
    ifId.valid = true;
    ifId.instruction = std::make_shared<Instruction>(instr);
    ifId.pc = pc;
    
    // Increment PC
    pc += 4;
}

void Processor::stageID() {
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
    idEx.rs1Value = registers.read(instr->getRs1());
    idEx.rs2Value = registers.read(instr->getRs2());
    
    // Check if this is a branch instruction and calculate target
    if (instr->isBranch() || instr->isJump()) {
        idEx.isBranch = true;
        
        // Different branch/jump types have different target calculations
        if (instr->isBType()) {
            idEx.branchTarget = idEx.pc + instr->getImm();
        } else if (instr->getOpcode() == 0x6F) { // JAL
            idEx.branchTarget = idEx.pc + instr->getImm();
            idEx.branchTaken = true; // JAL always takes the jump
        } else if (instr->getOpcode() == 0x67) { // JALR
            idEx.branchTarget = (idEx.rs1Value + instr->getImm()) & ~1; // Clear least significant bit
            idEx.branchTaken = true; // JALR always takes the jump
        }
    } else {
        idEx.isBranch = false;
        idEx.branchTaken = false;
    }
    
    // For branches, evaluate condition
    if (instr->isBType()) {
        int funct3 = instr->getFunct3();
        int rs1Val = idEx.rs1Value;
        int rs2Val = idEx.rs2Value;
        
        switch (funct3) {
            case 0x0: // BEQ
                idEx.branchTaken = (rs1Val == rs2Val);
                break;
            case 0x1: // BNE
                idEx.branchTaken = (rs1Val != rs2Val);
                break;
            case 0x4: // BLT
                idEx.branchTaken = (rs1Val < rs2Val);
                break;
            case 0x5: // BGE
                idEx.branchTaken = (rs1Val >= rs2Val);
                break;
            case 0x6: // BLTU
                idEx.branchTaken = ((unsigned int)rs1Val < (unsigned int)rs2Val);
                break;
            case 0x7: // BGEU
                idEx.branchTaken = ((unsigned int)rs1Val >= (unsigned int)rs2Val);
                break;
            default:
                idEx.branchTaken = false;
                break;
        }
    }
    
    // Handle branch prediction (simple always-not-taken strategy)
    if (idEx.isBranch && idEx.branchTaken) {
        // We're predicting not taken but branch is taken, flush and redirect
        ifId.clear();
        pc = idEx.branchTarget;
    }
}

void Processor::stageEX() {
    if (!idEx.valid) {
        exMem.clear();
        return;
    }
    
    // Copy values from ID/EX to EX/MEM
    exMem.instruction = idEx.instruction;
    exMem.pc = idEx.pc;
    exMem.valid = true;
    exMem.rs1Value = idEx.rs1Value;
    exMem.rs2Value = idEx.rs2Value;
    exMem.isBranch = idEx.isBranch;
    exMem.branchTaken = idEx.branchTaken;
    exMem.branchTarget = idEx.branchTarget;
    
    // Execute ALU operation
    auto instr = exMem.instruction;
    int aluResult = 0;
    
    if (instr->isRType()) {
        int funct3 = instr->getFunct3();
        int funct7 = instr->getFunct7();
        
        switch (funct3) {
            case 0x0: // ADD/SUB
                if (funct7 == 0x00)
                    aluResult = idEx.rs1Value + idEx.rs2Value; // ADD
                else if (funct7 == 0x20)
                    aluResult = idEx.rs1Value - idEx.rs2Value; // SUB
                break;
            case 0x1: // SLL
                aluResult = idEx.rs1Value << (idEx.rs2Value & 0x1F);
                break;
            case 0x2: // SLT
                aluResult = (idEx.rs1Value < idEx.rs2Value) ? 1 : 0;
                break;
            case 0x3: // SLTU
                aluResult = ((unsigned int)idEx.rs1Value < (unsigned int)idEx.rs2Value) ? 1 : 0;
                break;
            case 0x4: // XOR
                aluResult = idEx.rs1Value ^ idEx.rs2Value;
                break;
            case 0x5: // SRL/SRA
                if (funct7 == 0x00)
                    aluResult = (unsigned int)idEx.rs1Value >> (idEx.rs2Value & 0x1F); // SRL
                else if (funct7 == 0x20)
                    aluResult = idEx.rs1Value >> (idEx.rs2Value & 0x1F); // SRA
                break;
            case 0x6: // OR
                aluResult = idEx.rs1Value | idEx.rs2Value;
                break;
            case 0x7: // AND
                aluResult = idEx.rs1Value & idEx.rs2Value;
                break;
        }
    } else if (instr->isIType()) {
        int funct3 = instr->getFunct3();
        int imm = instr->getImm();
        
        if (instr->getOpcode() == 0x13) { // ALU with immediate
            switch (funct3) {
                case 0x0: // ADDI
                    aluResult = idEx.rs1Value + imm;
                    break;
                case 0x2: // SLTI
                    aluResult = (idEx.rs1Value < imm) ? 1 : 0;
                    break;
                case 0x3: // SLTIU
                    aluResult = ((unsigned int)idEx.rs1Value < (unsigned int)imm) ? 1 : 0;
                    break;
                case 0x4: // XORI
                    aluResult = idEx.rs1Value ^ imm;
                    break;
                case 0x6: // ORI
                    aluResult = idEx.rs1Value | imm;
                    break;
                case 0x7: // ANDI
                    aluResult = idEx.rs1Value & imm;
                    break;
                case 0x1: // SLLI
                    aluResult = idEx.rs1Value << (imm & 0x1F);
                    break;
                case 0x5: // SRLI/SRAI
                    if ((imm >> 5) == 0)
                        aluResult = (unsigned int)idEx.rs1Value >> (imm & 0x1F); // SRLI
                    else
                        aluResult = idEx.rs1Value >> (imm & 0x1F); // SRAI
                    break;
            }
        } else if (instr->getOpcode() == 0x03) { // Load
            // Calculate memory address
            aluResult = idEx.rs1Value + imm;
        } else if (instr->getOpcode() == 0x67) { // JALR
            // Store return address (PC+4)
            aluResult = idEx.pc + 4;
        }
    } else if (instr->isSType()) { // Store
        // Calculate memory address
        aluResult = idEx.rs1Value + instr->getImm();
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

void Processor::stageMEM() {
    if (!exMem.valid) {
        memWb.clear();
        return;
    }
    
    // Copy values from EX/MEM to MEM/WB
    memWb.instruction = exMem.instruction;
    memWb.pc = exMem.pc;
    memWb.valid = true;
    memWb.aluResult = exMem.aluResult;
    
    auto instr = memWb.instruction;
    
    // Memory operations
    if (instr->isLoad()) {
        int funct3 = instr->getFunct3();
        uint32_t address = exMem.aluResult;
        
        switch (funct3) {
            case 0x0: // LB - Load Byte
                memWb.readData = (int8_t)memory.readByte(address);
                break;
            case 0x1: // LH - Load Half
                memWb.readData = (int16_t)memory.readHalf(address);
                break;
            case 0x2: // LW - Load Word
                memWb.readData = (int32_t)memory.readWord(address);
                break;
            case 0x4: // LBU - Load Byte Unsigned
                memWb.readData = memory.readByte(address);
                break;
            case 0x5: // LHU - Load Half Unsigned
                memWb.readData = memory.readHalf(address);
                break;
        }
    } else if (instr->isStore()) {
        int funct3 = instr->getFunct3();
        uint32_t address = exMem.aluResult;
        int value = exMem.rs2Value;
        
        switch (funct3) {
            case 0x0: // SB - Store Byte
                memory.writeByte(address, value & 0xFF);
                break;
            case 0x1: // SH - Store Half
                memory.writeHalf(address, value & 0xFFFF);
                break;
            case 0x2: // SW - Store Word
                memory.writeWord(address, value);
                break;
        }
    }
}

void Processor::stageWB() {
    if (!memWb.valid) {
        return;
    }
    
    auto instr = memWb.instruction;
    int rdNum = instr->getRd();
    
    // Write back result to register file
    if (instr->isLoad()) {
        registers.write(rdNum, memWb.readData);
    } else if (instr->isRType() || 
              (instr->isIType() && instr->getOpcode() == 0x13) || // ALU immediate
              instr->isUType() || 
              instr->isJump()) {
        registers.write(rdNum, memWb.aluResult);
    }
    
    // Don't write back for store and branch instructions
}

// Helper function to strip comments and normalize instruction text
std::string Processor::stripComments(const std::string& assembly) {
    // Find position of comment start
    std::string result = assembly;
    size_t commentPos = result.find('#');
    if (commentPos != std::string::npos) {
        result = result.substr(0, commentPos);
    }
    
    // Trim leading whitespace
    size_t start = result.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return "NOP"; // Return "NOP" for empty lines or lines containing only whitespace
    }
    
    // Trim trailing whitespace
    size_t end = result.find_last_not_of(" \t\r\n");
    result = result.substr(start, end - start + 1);
    
    // Normalize multiple spaces to single space
    std::string normalizedResult;
    bool lastWasSpace = false;
    for (char c : result) {
        if (std::isspace(c)) {
            if (!lastWasSpace) {
                normalizedResult += ' ';
                lastWasSpace = true;
            }
        } else {
            normalizedResult += c;
            lastWasSpace = false;
        }
    }
    
    return normalizedResult;
}

void Processor::updatePipelineTable() {
    // Track all instructions in the pipeline for this cycle
    
    // Instruction in WB stage
    if (memWb.valid) {
        std::string instr = stripComments(memWb.instruction->getAssembly());
        updateOrAddInstruction(instr, "WB");
    }
    
    // Instruction in MEM stage
    if (exMem.valid) {
        std::string instr = stripComments(exMem.instruction->getAssembly());
        updateOrAddInstruction(instr, "MEM");
    }
    
    // Instruction in EX stage
    if (idEx.valid) {
        std::string instr = stripComments(idEx.instruction->getAssembly());
        updateOrAddInstruction(instr, "EX");
    }
    
    // Instruction in ID stage
    if (ifId.valid) {
        std::string instr = stripComments(ifId.instruction->getAssembly());
        updateOrAddInstruction(instr, "ID");
    }
    
    // Instruction in IF stage (if not stalled)
    if (!stall) {
        auto fetchedInstr = memory.getInstruction(pc);
        std::string instr = stripComments(fetchedInstr.getAssembly());
        updateOrAddInstruction(instr, "IF");
    } else {
        // When stalled, the instruction currently visible at pc is still in IF
        // We need to make sure it's shown as being in the pipeline
        if (cycleCount > 0) { // Not for the first cycle
            // Find the instruction that's stalled
            for (auto& tracker : pipelineTable) {
                // If the instruction was in IF in the previous cycle and isn't in any other stage now
                if (tracker.stages.size() > cycleCount && 
                    tracker.stages[cycleCount-1] == "IF" &&
                    tracker.stages.size() <= cycleCount + 1) {
                    // Mark it as still in IF
                    tracker.stages.resize(cycleCount + 1, "-");
                    tracker.stages[cycleCount] = "IF";
                    break;
                }
            }
        }
    }
    
    // Update all existing instructions to show they're not in any stage this cycle if necessary
    for (auto& tracker : pipelineTable) {
        if (tracker.stages.size() <= cycleCount) {
            tracker.stages.resize(cycleCount + 1, "-");
        }
    }
}

void Processor::updateOrAddInstruction(const std::string& assembly, const std::string& stage) {
    // Try to find the instruction in the table
    for (auto& tracker : pipelineTable) {
        if (tracker.assembly == assembly) {
            // Instruction already exists in the table
            // Make sure the stages vector is large enough for the current cycle
            if (tracker.stages.size() <= cycleCount) {
                tracker.stages.resize(cycleCount + 1, "-");
            }
            // Update the stage for this cycle
            tracker.stages[cycleCount] = stage;
            return;
        }
    }
    
    // Instruction not found, add a new tracker
    InstructionTracker newTracker;
    newTracker.assembly = assembly;
    newTracker.firstCycle = cycleCount;
    newTracker.stages.resize(cycleCount + 1, "-");
    newTracker.stages[cycleCount] = stage;
    pipelineTable.push_back(newTracker);
}

void Processor::printPipelineDiagram() {
    // Sort the instructions by the order they entered the pipeline
    std::sort(pipelineTable.begin(), pipelineTable.end(), 
              [](const InstructionTracker& a, const InstructionTracker& b) {
                  return a.firstCycle < b.firstCycle;
              });
    
    // Print each instruction with its pipeline stages
    for (const auto& tracker : pipelineTable) {
        std::string line = tracker.assembly;
        
        // Add each stage separated by semicolons
        for (const auto& stage : tracker.stages) {
            line += ";" + stage;
        }
        
        std::cout << line << std::endl;
    }
}

std::vector<std::string> Processor::generatePipelineDiagram() {
    std::vector<std::string> diagram;
    
    // Each line represents an instruction in the pipeline
    // Format: <assembly>;<IF>;<ID>;<EX>;<MEM>;<WB>
    
    // Check each pipeline register and generate the diagram
    if (memWb.valid) {
        std::string instr = stripComments(memWb.instruction->getAssembly());
        std::string line = instr + ";-;-;-;-;WB";
        diagram.push_back(line);
    }
    
    if (exMem.valid) {
        std::string instr = stripComments(exMem.instruction->getAssembly());
        std::string line = instr + ";-;-;-;MEM;-";
        diagram.push_back(line);
    }
    
    if (idEx.valid) {
        std::string instr = stripComments(idEx.instruction->getAssembly());
        std::string line = instr + ";-;-;EX;-;-";
        diagram.push_back(line);
    }
    
    if (ifId.valid) {
        std::string instr = stripComments(ifId.instruction->getAssembly());
        std::string line = instr + ";-;ID;-;-;-";
        diagram.push_back(line);
    }
    
    // Current instruction being fetched
    if (!stall) {
        auto fetchedInstr = memory.getInstruction(pc);
        std::string instr = stripComments(fetchedInstr.getAssembly());
        std::string line = instr + ";IF;-;-;-;-";
        diagram.push_back(line);
    }
    
    return diagram;
}


// addi x1, x0, 5;   IF;ID;EX;MEM;WB;-;-;-;-;-;-
// addi x2, x0, 10;  - ;IF;ID;EX;MEM;WB;-;-;-;-;-
// add x3, x1, x2;   - ;- ;IF;ID;ID;EX;MEM;WB;-;-;-
// sub x4, x1, x2;   - ;- ;- ;IF;-;ID;EX;MEM;WB;-;-
// xor x5, x1, x2;   - ;- ;- ;- ;- ;IF;ID;EX;MEM;WB;-
// or x6, x1, x2;    - ;- ;- ;- ;- ;- ;IF;ID;EX;MEM;WB
// and x7, x1, x2;   -;-;-;-;-;-;-;IF;ID;EX;MEM
// NOP;-;-;-;-;-;-;-;-;IF;IF;IF