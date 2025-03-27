#pragma once
#include "Processor.hpp"

class ForwardingProcessor : public Processor {
protected:
    // Override hazard detection to implement forwarding
    void detectHazards() override;
    
    // Override ID stage to implement forwarding for branches
    void stageID() override;
    
    // Override EX stage to implement forwarding
    void stageEX() override;
    
public:
    ForwardingProcessor();
    ~ForwardingProcessor() override = default;
};
