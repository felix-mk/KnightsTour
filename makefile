TARGET	:= tour

MAIN	:= main.cpp
HEADERS	:= $(wildcard *.hpp)

MAKEFILE := makefile

CXX			:= g++
CXXFLAGS	:= -Wall -Wextra -Wpedantic -Werror -std=c++2a -fno-exceptions -fno-rtti -O3 -march=native

$(TARGET): $(MAIN) $(HEADERS) $(MAKEFILE)
	$(CXX) $(CXXFLAGS) $(MAIN) -o $(TARGET)

.PHONY: clean
clean:
	$(RM) -rf $(TARGET) $(TARGET).exe
