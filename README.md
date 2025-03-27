# RISC-V Pipeline Simulator with Branch Taken Implementation

This project implements a RISC-V pipeline simulator that models a simplified processor with a five-stage pipeline. It includes two variants:

- **Non-Forwarding Processor:** Does not forward data. Rather, it avoids hazards by halting the pipeline until needed data is written back. Branch address decoding for this implementation is performed in the Instruction Decode (ID) stage.
- **Forwarding Processor:** Utilizes data forwarding to recover from data hazards in the Execute (EX) phase. Branch instructions are executed in the EX phase, leveraging the latest forwarded data.

---

## Overview

We designed the simulator keeping in mind the execution of actual RISC-V instructions. It aims to illustrate how hazards are handled and how design decisions affect performance and correctness. The simulator includes ID, IF, EX, MEM, WB stages.

---

## Pipeline Architecture

### Stages
The simulator models the following five stages:
1. **Instruction Fetch (IF):** Retrieves the instruction from memory using the current program counter (PC).
2. **Instruction Decode (ID):** Decodes the fetched instruction, reads the register file, and determines control signals.
     - In the non-forwarding processor, branch conditions are evaluated in this stage using already saved/written back data to register files. (Branch Target Finding)
3. **Execute (EX):** Performs arithmetic/logical operations and calculates branch targets.  
   - In the forwarding processor, branch conditions are evaluated in this stage using forwarded data.
4. **Memory Access (MEM):** Handles load and store operations. For load instructions, the effective memory address is computed, and data is fetched.
5. **Write Back (WB):** Writes results back to the register file.

### Pipeline Registers
Each stage communicates with the next using dedicated pipeline registers:
- **IF/ID:** Holds the fetched instruction and the PC.
- **ID/EX:** Contains the decoded instruction, source register values, and calculated branch target (if applicable).
- **EX/MEM:** Passes ALU results, branch decisions, and any computed memory addresses.
- **MEM/WB:** Carries the results to be written back to the register file.

These registers are essential for maintaining the state of the pipeline and for implementing the hazard detection and resolution mechanisms.

---

## Design Decisions and Implementation Details

### 1. Hazard Detection & Resolution

#### Non-Forwarding Processor
- **RAW Hazard Detection:**  
  In the absence of forwarding, the processor has to guarantee that instructions don't use outdated data. Hazard detection is detecting whether the source registers of the instruction in the ID stage are equal to the destination registers of instructions in the EX, MEM, or WB stages. In case a dependency is detected, the pipeline will stall until data is available.
  
- **Branch Address Decoding in ID Stage:**  
  In this variant, the branch target address is computed during the ID stage. If the branch instruction depends on a result that is yet to be computed (detected via hazard checks), the pipeline stalls.  
  *Example:*  
  A branch instruction (for example, BEQ) in the ID stage determines its branch target by adding the immediate value and the current PC (`idEx.pc + instr->getImm()`). If register values employed for computing the branch condition are stale because of a RAW hazard, the instruction does not execute until the hazard has been resolved.

#### Forwarding Processor
- **Load-Use Hazard Handling:**  
    When we need data in the ID stage that is being loaded in the EX stage, a load-use hazard is detected. In such cases, the simulator stalls by inserting a bubble (NOP) and also clears the ID/EX register so the dependent instruction is delayed.
  
- **Data Forwarding:**  
    The EX stage verifies any hazard by comparing the source registers of the current instruction with the destination registers of subsequent instructions (MEM/WB). If we identify the same register value that has not been written yet, we take the latest value and pass it to the EX stage. This makes sure that branch and ALU operations are executed with proper, latest/updated data.
  
- **Branch Evaluation in EX Stage:**  
Branch instructions are examined during the EX stage based on register values forwarded and decide on the target address of the branch if the branch has to be executed.  This reduces the penalty of a branch misprediction as the branch choice is made using the latest possible data. The real spec for this assignment mentions decoding at the ID stage, but we realized that by decoding at the EX stage, we reduced some of the stalls that could have been caused by decoding at the ID stage. There is, however, a trade-off here—some examples take longer cycles to run than others. Here is a sample where we employ fewer cycles than in the case of ID stage decoding. 

### 2. Branch Handling

- **Non-Forwarding Variant (ID Stage Decoding):**  
  Branch decoding occurs earlier, at the ID stage. However, if there is a hazard (for example, if the branch instruction’s source registers are waiting for data from previous instructions), the pipeline stalls until the correct values are available.
   *Example:*  
For the non-forwarding design, look at a branch instruction that waits on a previous instruction that has not yet advanced beyond the EX or MEM stage. The logic for detecting a hazard determines that there is dependency by matching up the source register numbers in the branch instruction against the destination register numbers in past instructions. Through this, the pipeline inserts a stall so that the branch target calculation and the condition evaluation within the ID stage employ the correct register values.

- **Forwarding Variant (EX Stage Decoding):**  
  Branch decisions are delayed until the EX stage, where the instruction can leverage forwarded values to compute the branch condition. This minimizes the number of stalls by ensuring that the decision is based on the most recent results.
   *Example:*  
1) `add x10, x0, 10`  
2) `add x11, x0, 10`  
3) `beq x10, x11, 8`  
4) Some instruction  
5) Some instruction (target of beq)  

If we decode the branch in the ID stage:
```
IF  ID  EX  MEM  WB  
-   IF  ID  EX  MEM  WB 
-   -   IF  ID  -    EX  MEM  WB  -> One stall if branch prediction is done in ID stage
```

If we do it in the EX stage, there is no such stall. Hence, we choose to do branch prediction in the EX stage. In the non-forwarding case, this does not affect performance, as values are taken after the WB stage, so ID stage decoding is better.

### 3. Modular Code Organization

The code is divided into multiple classes to isolate functionalities:
- **Processor Classes:**  
  - `Processor`: Base class that handles the overall pipeline operation.
  - `NonForwardingProcessor`: Inherits from `Processor` and implements stalling and ID stage branch address decoding.
  - `ForwardingProcessor`: Inherits from `Processor` and implements forwarding logic and EX stage branch evaluation.

- **Supporting Classes:**  
  - `Instruction`: Handles decoding of machine code into fields such as opcode, funct3, funct7, source/destination registers, and immediate values.
  - `Memory`: Simulates memory accesses for load and store operations.
  - `RegisterFile`: Interface for reading/writing to the register file.

### 4. Pipeline Table and Debugging

The simulator maintains a pipeline table that tracks which instruction is in each stage for every cycle. Debug statements are included to trace pipeline stages and observe hazard resolution.

---

## Usage

**Command Line Usage:**
```bash
./forward <instruction_file> <cycle_count>
./noforward <instruction_file> <cycle_count>
