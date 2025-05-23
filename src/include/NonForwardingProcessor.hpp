#pragma once
#include "Processor.hpp"
using namespace std;
class NonForwardingProcessor : public Processor {
protected:
    // Override hazard detection for stall implementation
    void detectHazards() override;
    
public:
    NonForwardingProcessor();
    ~NonForwardingProcessor() override = default;
};
