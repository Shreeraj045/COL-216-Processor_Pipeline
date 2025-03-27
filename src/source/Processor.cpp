#include "../include/Processor.hpp"
using namespace std;
Processor::Processor() : pc(0), cycleCount(0), instructionCount(0), stall(false) {
}

void Processor::loadProgram(const string& filename) {
    reset();
    memory.loadInstructions(filename);
    
    // Preload all instructions into the pipeline table
    for (uint32_t i = 0; i < memory.getInstructionCount(); i++) {
        uint32_t instrAddr = i * 4;
        auto instr = memory.getInstruction(instrAddr);
        string instrText = stripComments(instr.getAssembly());
        
        // Skip blank lines
        if (instrText.empty()) {
            continue;
        }
        
        // Add all instructions to the tracking table without any stages yet
        InstructionTracker newTracker;
        newTracker.assembly = instrText;
        newTracker.pc = instrAddr;  // Store the instruction's PC address
        newTracker.firstCycle = -1;  // -1 indicates not yet executed
        
        // If this is the first instruction, mark it as in IF stage for cycle 0
        if (i == 0) {
            newTracker.firstCycle = 0;
            newTracker.stages.resize(1, "IF");
        }
        
        pipelineTable.push_back(newTracker);
    }
}

void Processor::run(int cycles) {
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
    ifId.instruction = make_shared<Instruction>(instr);
    ifId.pc = pc;
    //print instruciton with cycle count 
    // string instrText = stripComments(instr.getAssembly());
    // cout<<"************************************************"<<endl;
    // cout<<"Instruction at IF: "<<instrText<<"cycle"<<cycleCount<<endl;
    // cout<<"************************************************"<<endl;

    // Increment PC
    pc += 4;

    if (tibt) {
        // cout<<"branch taken to pc: "<<btpc<<endl;
        ifId.clear();
        tibt = false ; 
        pc = btpc;
    }

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
    

    idEx.instruction = ifId.instruction;
    idEx.pc = ifId.pc;
    idEx.valid = true;
    
    // Read register values
    auto instr = idEx.instruction;
    idEx.rs1Value = registers.read(instr->getRs1());
    idEx.rs2Value = registers.read(instr->getRs2());
    
    // Check if this is a branch instruction and calculate target
    if (instr->isBType() || instr->isJump()) {
        idEx.isBType = true;
        
        // Different branch/jump types have different target calculations
        if (instr->isBType()) {
            // The immediate value in branch instructions represents the byte offset directly
            // In RISC-V, the immediate value is already multiplied by 2 during encoding
            // So we should use it directly without any scaling
            idEx.branchTarget = idEx.pc + instr->getImm();
        } else if (instr->getOpcode() == 0x6F) { // JAL
            idEx.branchTarget = idEx.pc + instr->getImm();
            idEx.branchTaken = true; // JAL always takes the jump
        } else if (instr->getOpcode() == 0x67) { // JALR
            idEx.branchTarget = (idEx.rs1Value + instr->getImm()) & ~1; // Clear least significant bit
            idEx.branchTaken = true; // JALR always takes the jump
        }
    } else {
        idEx.isBType = false;
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
                // cout<<"BEQ with rs1Val: "<<rs1Val<<" rs2Val: "<<rs2Val<<endl;
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
    if (idEx.isBType && idEx.branchTaken) {
        // We're predicting not taken but branch is taken, flush and redirect
        ifId.clear();
        // pc = idEx.branchTarget;
        btpc = idEx.branchTarget;
        tibt = true ; 
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
    exMem.isBType = idEx.isBType;
    exMem.branchTaken = idEx.branchTaken;
    exMem.branchTarget = idEx.branchTarget;
    
    // Execute ALU operation
    auto instr = exMem.instruction;
    int aluResult = 0;
    
    if (instr->isRType()) {
        int funct3 = instr->getFunct3();
        int funct7 = instr->getFunct7();
        
        // Check if this is an M-extension instruction
        if (funct7 == 0x01) {
            // M-extension instructions (MUL/DIV/REM)
            switch (funct3) {
                case 0x0: // MUL
                    aluResult = idEx.rs1Value * idEx.rs2Value;
                    break;
                case 0x1: // MULH
                    // Signed * Signed -> High bits
                    {
                        int64_t a = static_cast<int64_t>(idEx.rs1Value);
                        int64_t b = static_cast<int64_t>(idEx.rs2Value);
                        int64_t result = a * b;
                        aluResult = static_cast<int>(result >> 32);
                    }
                    break;
                case 0x2: // MULHSU
                    // Signed * Unsigned -> High bits
                    {
                        int64_t a = static_cast<int64_t>(idEx.rs1Value);
                        uint64_t b = static_cast<uint64_t>(static_cast<uint32_t>(idEx.rs2Value));
                        int64_t result = a * b;
                        aluResult = static_cast<int>(result >> 32);
                    }
                    break;
                case 0x3: // MULHU
                    // Unsigned * Unsigned -> High bits
                    {
                        uint64_t a = static_cast<uint64_t>(static_cast<uint32_t>(idEx.rs1Value));
                        uint64_t b = static_cast<uint64_t>(static_cast<uint32_t>(idEx.rs2Value));
                        uint64_t result = a * b;
                        aluResult = static_cast<int>(result >> 32);
                    }
                    break;
                case 0x4: // DIV
                    // Check for division by zero
                    if (idEx.rs2Value == 0) {
                        aluResult = -1; // As per spec: division by zero returns -1
                    } 
                    // Check for overflow condition (INT_MIN / -1)
                    else if (idEx.rs1Value == INT_MIN && idEx.rs2Value == -1) {
                        aluResult = INT_MIN; // Return INT_MIN as specified
                    } 
                    else {
                        aluResult = idEx.rs1Value / idEx.rs2Value;
                    }
                    break;
                case 0x5: // DIVU
                    // Unsigned division
                    if (idEx.rs2Value == 0) {
                        aluResult = 0xFFFFFFFF; // Max unsigned value for division by zero
                    } else {
                        aluResult = static_cast<int>((static_cast<uint32_t>(idEx.rs1Value) / 
                                                     static_cast<uint32_t>(idEx.rs2Value)));
                    }
                    break;
                case 0x6: // REM
                    // Remainder of signed division
                    if (idEx.rs2Value == 0) {
                        aluResult = idEx.rs1Value; // Remainder of x/0 is x
                    } 
                    // Handle overflow case (INT_MIN % -1)
                    else if (idEx.rs1Value == INT_MIN && idEx.rs2Value == -1) {
                        aluResult = 0; // Remainder is 0 in this case
                    } 
                    else {
                        aluResult = idEx.rs1Value % idEx.rs2Value;
                    }
                    break;
                case 0x7: // REMU
                    // Remainder of unsigned division
                    if (idEx.rs2Value == 0) {
                        aluResult = idEx.rs1Value; // Remainder of x/0 is x
                    } else {
                        aluResult = static_cast<int>((static_cast<uint32_t>(idEx.rs1Value) % 
                                                     static_cast<uint32_t>(idEx.rs2Value)));
                    }
                    break;
            }
        } else {
            // Regular R-type instructions
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
    } else if (instr->isSType()) {
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

    // cout<<"************************************************"<<endl;
    // cout<<"Instruction at WB: "<<stripComments(instr->getAssembly())<<"cycle"<<cycleCount<<endl;
    // cout<<"************************************************"<<endl;


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
    //print just finished WB instruction with cycle count
}

// Helper function to strip comments and normalize instruction text
string Processor::stripComments(const string& assembly) {
    // Find position of comment start
    string result = assembly;
    size_t commentPos = result.find('#');
    if (commentPos != string::npos) {
        result = result.substr(0, commentPos);
    }
    
    // Trim leading whitespace
    size_t start = result.find_first_not_of(" \t\r\n");
    if (start == string::npos) {
        return "NOP"; // Return "NOP" for empty lines or lines containing only whitespace
    }
    
    // Trim trailing whitespace
    size_t end = result.find_last_not_of(" \t\r\n");
    result = result.substr(start, end - start + 1);
    
    // Normalize multiple spaces to single space
    string normalizedResult;
    bool lastWasSpace = false;
    for (char c : result) {
        if (isspace(c)) {
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
    // Track all instructions in the pipeline for this cycle based on their PC addresses
    
    // Instruction in WB stage
    if (memWb.valid) {
        uint32_t instrPC = memWb.pc;
        updateInstructionStage(instrPC, "WB");
    }
    
    // Instruction in MEM stage
    if (exMem.valid) {
        uint32_t instrPC = exMem.pc;
        updateInstructionStage(instrPC, "MEM");
    }
    
    // Instruction in EX stage
    if (idEx.valid) {
        uint32_t instrPC = idEx.pc;
        updateInstructionStage(instrPC, "EX");
    }
    
    // Instruction in ID stage - Only update if not stalled
    if (ifId.valid && !stall) {
        uint32_t instrPC = ifId.pc;
        updateInstructionStage(instrPC, "ID");
    }
    
    // Instruction in IF stage
    if (!stall && pc < memory.getInstructionCount() * 4) {
        // Only track IF for valid PC addresses within program memory
        updateInstructionStage(pc, "IF");
    } else if (stall && ifId.valid) {
        // When stalled, no new instruction enters IF
        // The instruction in IF/ID stays in IF stage
        // No need to do anything special here
    }
    
    // Update all existing instructions to show they're not in any stage this cycle if necessary
    for (auto& tracker : pipelineTable) {
        if (tracker.stages.size() <= static_cast<size_t>(cycleCount)) {
            tracker.stages.resize(cycleCount + 1, "-");
        }
    }
}

void Processor::updateInstructionStage(uint32_t pc, const string& stage) {
    // Find the instruction with matching PC in the table
    for (auto& tracker : pipelineTable) {
        if (tracker.pc == pc) {
            // If this is the first time we're seeing this instruction executed
            if (tracker.firstCycle == -1) {
                tracker.firstCycle = cycleCount;
            }
            
            // Make sure the stages vector is large enough for the current cycle
            if (tracker.stages.size() <= static_cast<size_t>(cycleCount)) {
                tracker.stages.resize(cycleCount + 1, "-");
            }
            
            // Check if the current stage is the same as the previous cycle's stage
            bool sameAsPrevious = false;
            if (cycleCount > 0 && tracker.stages.size() > static_cast<size_t>(cycleCount - 1)) {
                sameAsPrevious = (tracker.stages[cycleCount - 1] == stage);
            }
            
            // Update the stage for this cycle
            if (tracker.stages[cycleCount] == "-") {
                // If the stage is the same as previous cycle, mark with "-"
                if (sameAsPrevious) {
                    tracker.stages[cycleCount] = "-";
                } else {
                    tracker.stages[cycleCount] = stage;
                }
            } else {
                // Combine stages if there's already a stage for this cycle
                // Only add if not already a duplicate
                if (!sameAsPrevious) {
                    tracker.stages[cycleCount] += "/" + stage;
                }
            }
            return;
        }
    }
    
    // If somehow the instruction wasn't preloaded, add it now
    if (pc < memory.getInstructionCount() * 4) {
        auto instr = memory.getInstruction(pc);
        string instrText = stripComments(instr.getAssembly());
        
        InstructionTracker newTracker;
        newTracker.assembly = instrText;
        newTracker.pc = pc;
        newTracker.firstCycle = cycleCount;
        newTracker.stages.resize(cycleCount + 1, "-");
        newTracker.stages[cycleCount] = stage;
        pipelineTable.push_back(newTracker);
    }
}

void Processor::printPipelineDiagram() {
    // Find the maximum length of any assembly instruction for alignment
    size_t maxInstrLength = 15; // Minimum width for instruction column
    for (const auto& tracker : pipelineTable) {
        maxInstrLength = max(maxInstrLength, tracker.assembly.length() + 10); // Add extra space for PC
    }
    
    // Find the maximum length of any stage string for proper column sizing
    size_t maxStageLength = 2; // Default width for single stages
    for (const auto& tracker : pipelineTable) {
        for (const auto& stage : tracker.stages) {
            maxStageLength = max(maxStageLength, stage.length());
        }
    }
    
    // Define the column width based on the longest stage
    const int cycleColWidth = maxStageLength + 3; // Add padding
    
    // Print cycle numbers at the top
    cout << left << setw(maxInstrLength) << "Instruction (PC)";
    for (int i = 0; i < cycleCount; i++) {
        string cycleHeader = "; C" + to_string(i);
        cout << left << setw(cycleColWidth) << cycleHeader;
    }
    cout << endl;
    
    // Print a separator line
    cout << string(maxInstrLength + cycleCount * cycleColWidth, '-') << endl;
    
    // Sort instructions by their PC for a logical ordering
    vector<InstructionTracker> sortedTrackers = pipelineTable;
    sort(sortedTrackers.begin(), sortedTrackers.end(), 
              [](const InstructionTracker& a, const InstructionTracker& b) {
                  return a.pc < b.pc;
              });
    
    // Print ALL instructions regardless of whether they entered the pipeline
    for (const auto& tracker : sortedTrackers) {
        // Format instruction with PC
        ostringstream instrWithPC;
        instrWithPC << tracker.assembly << " (" << dec << tracker.pc << ")";
        cout << left << setw(maxInstrLength) << instrWithPC.str();
        
        // Add each stage for each cycle
        for (size_t i = 0; i < static_cast<size_t>(cycleCount); i++) {
            string stageOutput = "; ";
            
            if (tracker.firstCycle != -1 && i < tracker.stages.size()) {
                stageOutput += tracker.stages[i];
            } else {
                stageOutput += "-"; // Changed from space to dash so all instructions are shown with '-'
            }
            
            cout << left << setw(cycleColWidth) << stageOutput;
        }
        cout << endl;
    }
}

void Processor::updateOrAddInstruction(const string& assembly, const string& stage) {
    // Trivial implementation: you can adapt it to your logic
    // For now, just record a new instruction tracker entry
    InstructionTracker instrTracker;
    instrTracker.assembly = assembly;
    instrTracker.pc = 0;      // no specific PC since we don't have it here
    instrTracker.firstCycle = cycleCount;
    instrTracker.stages.resize(cycleCount + 1, "-");
    instrTracker.stages[cycleCount] = stage;
    pipelineTable.push_back(instrTracker);
}

