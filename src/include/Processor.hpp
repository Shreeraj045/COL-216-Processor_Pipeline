#pragma once
#include "Memory.hpp"
#include "RegisterFile.hpp"
#include "PipelineRegister.hpp"
#include <vector>
#include <string>
#include <map>

class Processor {
protected:
    // Processor state
    uint32_t pc;
    Memory memory;
    RegisterFile registers;
    
    // Pipeline registers
    PipelineRegister ifId;
    PipelineRegister idEx;
    PipelineRegister exMem;
    PipelineRegister memWb;
    
    // Statistics
    int cycleCount;
    int instructionCount;
    
    // Internal tracking
    bool stall;
    
    // Structure to track instruction stages through all cycles
    struct InstructionTracker {
        std::string assembly;
        std::vector<std::string> stages; // Each position represents a cycle
        int firstCycle; // First cycle when instruction entered pipeline
    };
    
    // Table to track all instructions
    std::vector<InstructionTracker> pipelineTable;
    
    // Pipeline stage implementation
    virtual void stageIF();
    virtual void stageID();
    virtual void stageEX();
    virtual void stageMEM();
    virtual void stageWB();
    
    // Hazard detection and handling
    virtual void detectHazards() = 0;
    
    // Update pipeline table with current state
    void updatePipelineTable();
    
    // Helper to add or update instruction in table
    void updateOrAddInstruction(const std::string& assembly, const std::string& stage);
    
    // Helper function to strip comments from assembly code
    std::string stripComments(const std::string& assembly);
    
    
public:
    Processor();
    virtual ~Processor() = default;
    
    // Initialize the processor with instructions from a file
    void loadProgram(const std::string& filename);
    
    // Run the simulation for specified number of cycles
    void run(int cycles);
    
    // Reset processor state
    void reset();
    
    // Print the complete pipeline diagram
    void printPipelineDiagram();
};
