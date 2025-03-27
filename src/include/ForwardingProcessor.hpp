#pragma once
#include "Processor.hpp"
using namespace std;
class ForwardingProcessor : public Processor {
protected:
    //overriding hazard detection for forwarding processor 
    void detectHazards() override;
    
    // ID stage don't detect branch address (like in RIPES simulator)
    void stageID() override;
    
    // EX stage in forwarding detect branch address (if taken) (like in RIPES simulator)
    void stageEX() override;
    
public:
    ForwardingProcessor();
    ~ForwardingProcessor() override = default;
};
