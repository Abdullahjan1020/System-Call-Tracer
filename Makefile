CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++17

mytrace: src/main.cpp src/syscall_names.h
	$(CXX) $(CXXFLAGS) -o mytrace src/main.cpp

clean:
	rm -f mytrace