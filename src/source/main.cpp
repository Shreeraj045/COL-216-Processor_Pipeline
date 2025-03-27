#include "../include/ForwardingProcessor.hpp"
#include "../include/NonForwardingProcessor.hpp"

void printUsage(const std::string& progName) {
    std::cerr << "Usage: " << progName << " <instruction_file> <cycle_count>\n";
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        printUsage(argv[0]);
        return 1;
    }
    
    std::string filename = argv[1];
    int cycles;
    
    try {
        cycles = std::stoi(argv[2]);
    } catch (const std::exception&) {
        std::cerr << "Error: Cycle count must be a valid integer\n";
        return 1;
    }
    
    if (cycles <= 0) {
        std::cerr << "Error: Cycle count must be positive\n";
        return 1;
    }
    
    // Determine which processor variant to use based on executable name
    std::string exeName = argv[0];
    std::string::size_type lastSlash = exeName.find_last_of("/\\");
    if (lastSlash != std::string::npos) {
        exeName = exeName.substr(lastSlash + 1);
    }
    
    std::unique_ptr<Processor> processor;
    if (exeName == "forward") {
        processor = std::make_unique<ForwardingProcessor>();
    } else if (exeName == "noforward") {
        processor = std::make_unique<NonForwardingProcessor>();
    } else {
        std::cerr << "Error: Unknown executable name. Expected 'forward' or 'noforward'.\n";
        return 1;
    }
    
    try {
        processor->loadProgram(filename);
        processor->run(cycles);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    
    return 0;
}
