CXX = g++
CXXFLAGS = -std=c++17 -Wall

all:
	$(CXX) $(CXXFLAGS) src/*.cpp -o memory_simulator

clean:
	rm -f memory_simulator
