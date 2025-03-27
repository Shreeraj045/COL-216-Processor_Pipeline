#include "../include/Memory.hpp"


Memory::Memory(size_t size) : data(size, 0) {
}

uint8_t Memory::readByte(uint32_t address) const {
    if (address >= data.size()) {
        throw std::out_of_range("Memory address out of bounds");
    }
    return data[address];
}

uint16_t Memory::readHalf(uint32_t address) const {
    if (address + 1 >= data.size()) {
        throw std::out_of_range("Memory address out of bounds");
    }
    return static_cast<uint16_t>(data[address]) |
           (static_cast<uint16_t>(data[address + 1]) << 8);
}

uint32_t Memory::readWord(uint32_t address) const {
    if (address + 3 >= data.size()) {
        throw std::out_of_range("Memory address out of bounds");
    }
    return static_cast<uint32_t>(data[address]) |
           (static_cast<uint32_t>(data[address + 1]) << 8) |
           (static_cast<uint32_t>(data[address + 2]) << 16) |
           (static_cast<uint32_t>(data[address + 3]) << 24);
}

void Memory::writeByte(uint32_t address, uint8_t value) {
    if (address >= data.size()) {
        throw std::out_of_range("Memory address out of bounds");
    }
    data[address] = value;
}

void Memory::writeHalf(uint32_t address, uint16_t value) {
    if (address + 1 >= data.size()) {
        throw std::out_of_range("Memory address out of bounds");
    }
    data[address] = value & 0xFF;
    data[address + 1] = (value >> 8) & 0xFF;
}

void Memory::writeWord(uint32_t address, uint32_t value) {
    if (address + 3 >= data.size()) {
        throw std::out_of_range("Memory address out of bounds");
    }
    data[address] = value & 0xFF;
    data[address + 1] = (value >> 8) & 0xFF;
    data[address + 2] = (value >> 16) & 0xFF;
    data[address + 3] = (value >> 24) & 0xFF;
}

void Memory::loadInstructions(const std::string& filename) {
    instructions.clear();
    
    std::ifstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open instruction file: " + filename);
    }
    
    std::string line;
    while (std::getline(file, line)) {
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') {
            continue;
        }
        
        std::istringstream iss(line);
        std::string machineCodeStr;
        std::string assembly;
        
        // Read the first token as machine code
        iss >> machineCodeStr;
        
        // Convert machine code string to uint32_t as hexadecimal
        uint32_t machineCode;
        std::stringstream ss;
        ss << std::hex << machineCodeStr;
        ss >> machineCode;
        
        // Get rest of line as assembly code
        std::getline(iss >> std::ws, assembly);
        
        // Remove leading/trailing whitespace from assembly
        assembly.erase(0, assembly.find_first_not_of(" \t\r\n"));
        assembly.erase(assembly.find_last_not_of(" \t\r\n") + 1);
        
        instructions.emplace_back(machineCode, assembly);
    }
    
    if (instructions.empty()) {
        throw std::runtime_error("No valid instructions found in file: " + filename);
    }
}

Instruction Memory::getInstruction(uint32_t pc) const {
    size_t index = pc / 4;
    if (index >= instructions.size()) {
        return Instruction(); // Return NOP if beyond instruction memory
    }
    return instructions[index];
}

void Memory::reset() {
    std::fill(data.begin(), data.end(), 0);
    instructions.clear();
}
