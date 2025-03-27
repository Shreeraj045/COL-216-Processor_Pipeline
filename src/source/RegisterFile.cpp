#include "../include/RegisterFile.hpp"
using namespace std;
RegisterFile::RegisterFile() {
    reset();
}

int RegisterFile::read(int regNum) const {
    if (regNum == 0) return 0;  // x0 is hardwired to 0
    if (regNum < 0 || regNum >= 32) return 0;  // Invalid register
    return registers[regNum];
}

void RegisterFile::write(int regNum, int value) {
    if (regNum == 0) return;  // x0 can't be modified
    if (regNum < 0 || regNum >= 32) return;  // Invalid register
    registers[regNum] = value;
}

void RegisterFile::reset() {
    for (int i = 0; i < 32; ++i) {
        registers[i] = 0;
    }
}
