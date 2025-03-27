#include "../include/ForwardingProcessor.hpp"
#include "../include/NonForwardingProcessor.hpp"
using namespace std;

void printUsage(const string& progName) {
    cerr << "Usage: " << progName << " <instruction_file> <cycle_count>\n";
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        printUsage(argv[0]);
        return 1;
    }
    
    string filename = argv[1];
    int cycles;
    
    try {
        cycles = stoi(argv[2]);
    } catch (const exception&) {
        cerr << "Error: Cycle count must be a valid integer\n";
        return 1;
    }
    
    if (cycles <= 0) {
        cerr << "Error: Cycle count must be positive\n";
        return 1;
    }
    
    // which processor - forwarding or non-forwarding
    string exeName = argv[0];
    string::size_type lastSlash = exeName.find_last_of("/\\");
    if (lastSlash != string::npos) {
        exeName = exeName.substr(lastSlash + 1);
    }
    
    unique_ptr<Processor> processor;

    //make the call acoording to given processor type
    if (exeName == "forward") {
        processor = make_unique<ForwardingProcessor>();
    } else if (exeName == "noforward") {
        processor = make_unique<NonForwardingProcessor>();
    } else {
        cerr << "Error: Unknown executable name. Expected 'forward' or 'noforward'.\n";
        return 1;
    }
    
    try {
        processor->loadProgram(filename);
        processor->run(cycles);
    } catch (const exception& e) {
        cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    
    return 0;
}
