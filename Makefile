# Compiler and flags
CXX := g++
CXXFLAGS := -std=c++17 -Wall -Iinclude

# Folders
SRC_DIR := src
BUILD_DIR := build
INCLUDE_DIR := include
TARGET := $(BUILD_DIR)/mini-docker

# Source and object files
SRCS := $(wildcard $(SRC_DIR)/*.cpp)
OBJS := $(patsubst $(SRC_DIR)/%.cpp, $(BUILD_DIR)/%.o, $(SRCS))

# Default rule
all: $(TARGET)

# Compile .cpp files to .o
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Link the final executable
$(TARGET): $(OBJS)
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $^

# Clean up
clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean
