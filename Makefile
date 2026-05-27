CXX := g++
CXXFLAGS := -std=c++11 -Wall -Wextra -O2
LDFLAGS := -pthread

TARGET := build/ludo-engine
SOURCE := src/ludo_engine.cpp

.PHONY: all run clean

all: $(TARGET)

$(TARGET): $(SOURCE)
	mkdir -p build
	$(CXX) $(CXXFLAGS) $(SOURCE) -o $(TARGET) $(LDFLAGS)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -rf build
