#include "../include/NonForwardingProcessor.hpp"
using namespace std;
NonForwardingProcessor::NonForwardingProcessor() : Processor() {
}

void NonForwardingProcessor::detectHazards() {
    // in non-forwarding, stall until the dependent instruction completes WB stage
    stall = false;
    
    if (!ifId.valid) {
        return;
    }
    
    auto idInstr = ifId.instruction;
    
    // Check for read-after-write hazards with instructions in EX stage
    if (idEx.valid && idEx.instruction->getRd() != 0) {
        int exDest = idEx.instruction->getRd();
        
        // check if either source register of ID depends on EX destination
        if (idInstr->getRs1() == exDest || 
            (idInstr->getRs2() == exDest && !idInstr->isIType() && !idInstr->isUType() && !idInstr->isJType())) {
            // RAW hazard detected, stall the pipeline
            stall = true;
            return;
        }
    }
    
    // Check for RAW hazards with instructions in MEM stage
    if (exMem.valid && exMem.instruction->getRd() != 0) {
        int memDest = exMem.instruction->getRd();
        
        // check if either source register of ID depends on MEM destination
        if (idInstr->getRs1() == memDest || 
            (idInstr->getRs2() == memDest && !idInstr->isIType() && !idInstr->isUType() && !idInstr->isJType())) {
            // RAW hazard detected, stall the pipeline
            stall = true;
            return;
        }
    }
    
    // check for RAW hazards with instructions in WB stage
    if (memWb.valid && memWb.instruction->getRd() != 0) {
        int wbDest = memWb.instruction->getRd();
        
        // Check if either source register of ID depends on WB destination
        if (idInstr->getRs1() == wbDest || 
            (idInstr->getRs2() == wbDest && !idInstr->isIType() && !idInstr->isUType() && !idInstr->isJType())) {
            // RAW hazard detected, stall the pipeline
            stall = true;
            return;
        }
    }
}
