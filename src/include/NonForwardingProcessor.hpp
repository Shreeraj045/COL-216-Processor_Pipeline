#pragma once
#include "Processor.hpp"

class NonForwardingProcessor : public Processor {
protected:
    // Override hazard detection for stall implementation
    void detectHazards() override;
    
public:
    NonForwardingProcessor();
    ~NonForwardingProcessor() override = default;
};
