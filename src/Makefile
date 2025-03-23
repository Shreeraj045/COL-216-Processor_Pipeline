CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -pedantic -g

SRC_DIR = source
INCLUDE_DIR = include
BUILD_DIR = build

# Source files
SOURCES = $(SRC_DIR)/main.cpp \
          $(SRC_DIR)/Memory.cpp \
          $(SRC_DIR)/RegisterFile.cpp \
          $(SRC_DIR)/Instruction.cpp \
          $(SRC_DIR)/Processor.cpp \
          $(SRC_DIR)/ForwardingProcessor.cpp \
          $(SRC_DIR)/NonForwardingProcessor.cpp

# Object files
OBJS = $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(SOURCES))

# Targets
all: forward noforward

forward: $(OBJS)
	$(CXX) $(CXXFLAGS) -o forward $(OBJS) -DFORWARDING=1

noforward: $(OBJS)
	$(CXX) $(CXXFLAGS) -o noforward $(OBJS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@ -I$(INCLUDE_DIR)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR) forward noforward

.PHONY: all clean forward noforward
