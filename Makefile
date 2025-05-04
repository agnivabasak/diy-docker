# Compiler and flags
CXX := g++
CXXFLAGS := -std=c++17 -Wall -I/usr/include
LDFLAGS  = -l curl

# Need libcurl  - sudo apt install libcurl4-openssl-dev 
# Need nlohmann:json - sudo apt install nlohmann-json3-dev
# Need tar - sudo apt install tar

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
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

# Clean up
clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean
