
PREFIX ?= /usr/local/bin
CC ?= gcc
CXX ?= g++
CP ?= /bin/cp

LIBS += -lc
FLAGS += --std=c++17 -fno-exceptions

all:
	mkdir -p bin 
	$(CXX) $(FLAGS) -o bin/vpktool $(wildcard *.cpp)

rebuild: clean all 

clean:
	rm -rf bin

install: all 
	$(CP) -vf bin/vpktool $(PREFIX)/vpktool 