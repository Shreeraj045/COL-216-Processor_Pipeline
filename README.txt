# RISC-V Processor Pipeline Implementation

## Overview

This project implements a 5-stage pipelined RISC-V processor with forwarding capabilities. The five stages are:
1. Instruction Fetch (IF)
2. Instruction Decode (ID)
3. Execute (EX)
4. Memory Access (MEM)
5. Write Back (WB)

The implementation includes two processor variants:
- Basic pipelined processor (Processor.cpp)
- Data forwarding processor (ForwardingProcessor.cpp)

## Design Decisions and Implementation Details

### 1. Pipeline Structure

Each pipeline stage has its own dedicated method in the Processor class:
- stageIF(): Fetches instructions from memory
- stageID(): Decodes instructions and reads register values
- stageEX(): Executes operations (ALU, branch evaluation)
- stageMEM(): Performs memory operations
- stageWB(): Writes results back to registers

Pipeline registers between stages:
- IF/ID: Stores fetched instruction
- ID/EX: Stores decoded instruction and register values
- EX/MEM: Stores execution results
- MEM/WB: Stores data read from memory

### 2. Data Forwarding Implementation

One of the key design decisions was to implement data forwarding to reduce pipeline stalls. The forwarding mechanism:

- Forwards values from EX/MEM and MEM/WB stages back to the EX stage
- Detects when a register value is being computed by an earlier instruction
- Bypasses the normal register file read to use the most recent value

Implementation in ForwardingProcessor::stageEX():
```cpp
// Forward from MEM/WB first (older instruction)
if (memWb.valid && memWb.instruction) {
    int memWbRd = memWb.instruction->getRd();
    
    if (memWbRd != 0) {
        int wbValue = memWb.instruction->isLoad() ? memWb.readData : memWb.aluResult;
        
        if (rs1 == memWbRd) {
            rs1Value = wbValue;
            cout << "EX STAGE: Forwarded MEM/WB to rs1, new value=" << rs1Value << endl;
        }
        if (rs2 == memWbRd) {
            rs2Value = wbValue;
            cout << "EX STAGE: Forwarded MEM/WB to rs2, new value=" << rs2Value << endl;
        }
    }
}
```

### 3. Branch Handling

We moved branch evaluation from ID to EX stage in the forwarding processor to enable the use of forwarded values for branch decisions.

Key decision points:
- Branch target calculation in the EX stage
- Immediate branch redirection when a branch is taken
- Pipeline flushing to eliminate incorrect instructions

Implementation in ForwardingProcessor::stageEX():
```cpp
if (instr->isBType()) {
    // Calculate branch target
    exMem.branchTarget = idEx.pc + instr->getImm();
    
    // Evaluate branch condition with forwarded values
    int funct3 = instr->getFunct3();
    
    switch (funct3) {
        case 0x0: // BEQ
            exMem.branchTaken = (rs1Value == rs2Value);
            // ...other branch types...
    }
    
    // Handle branch decision immediately
    if (exMem.branchTaken) {
        // Branch is taken, flush pipeline and redirect
        ifId.clear();
        idEx.clear();
        pc = exMem.branchTarget;
    }
}
```

### 4. Hazard Detection

We implemented hazard detection to handle cases where forwarding cannot resolve dependencies:

1. Load-Use Hazards: When an instruction tries to use a value being loaded from memory
2. Control Hazards: When a branch changes the program flow

Implementation in ForwardingProcessor::detectHazards():
```cpp
if (idEx.valid && idEx.instruction && idEx.instruction->isLoad()) {
    int loadDest = idEx.instruction->getRd();
    
    if ((idInstr->getRs1() == loadDest && loadDest != 0) || 
        (idInstr->getRs2() == loadDest && loadDest != 0 && 
         !idInstr->isIType() && !idInstr->isUType() && !idInstr->isJType())) {
        // Load-use hazard detected, stall the pipeline
        stall = true;
        idEx.clear(); // Insert a bubble in ID/EX
    }
}
```

### 5. RISC-V Instruction Decoding

We implemented a full RISC-V instruction decoder in the Instruction class that:
- Extracts all instruction fields (opcode, rs1, rs2, rd, funct3, funct7, imm)
- Determines instruction type (R-type, I-type, S-type, B-type, U-type, J-type)
- Calculates immediate values based on instruction format

Special care was taken to ensure proper handling of JAL and JALR instructions:
```cpp
else if (instr->isJump() || instr->getOpcode() == 0x6F) {
    // For jumps
    if (instr->getOpcode() == 0x6F) { // JAL
        exMem.branchTarget = idEx.pc + instr->getImm();
        aluResult = idEx.pc + 4; // Return address
    } else if (instr->getOpcode() == 0x67) { // JALR
        exMem.branchTarget = (rs1Value + instr->getImm()) & ~1; // Clear LSB
        aluResult = idEx.pc + 4; // Return address
    }
    
    // Jumps are always taken
    exMem.branchTaken = true;
    
    // Flush and redirect
    ifId.clear();
    idEx.clear();
    pc = exMem.branchTarget;
}
```

### 6. Pipeline Visualization

A detailed pipeline visualization system was implemented to show the flow of instructions through the pipeline:
- Tracks each instruction's progress through the pipeline stages
- Handles visualization of stalls and flushes
- Shows multiple instructions in the same stage when needed

Example pipeline output:
```
