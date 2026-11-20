CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wpedantic

ifeq ($(OS),Windows_NT)
LDLIBS = -lws2_32
else
LDLIBS = -pthread
endif

all: huffz tests/tests

# Builds huffz with embedded HTTP server support
huffz: src/main.cpp src/*.hpp
	$(CXX) $(CXXFLAGS) -DHUFFZ_SERVER -o $@ src/main.cpp $(LDLIBS)

# Engine-only build without server dependencies (compatible with C++14)
cli: src/main.cpp src/*.hpp
	$(CXX) -std=c++14 -O2 -Wall -Wextra -Wpedantic -o huffz src/main.cpp

tests/tests: tests/tests.cpp src/*.hpp
	$(CXX) -std=c++14 -O2 -Wall -Wextra -Wpedantic -o $@ tests/tests.cpp

test: tests/tests
	./tests/tests

# Debug build with sanitizers: run this before trusting any bit-twiddling code.
sanitize:
	$(CXX) -std=c++17 -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer -o tests/tests_san tests/tests.cpp
	./tests/tests_san

clean:
	rm -f huffz tests/tests tests/tests_san
