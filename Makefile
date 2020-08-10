
PREFIX ?= /usr/local/
CC ?= gcc
CXX ?= g++
CP ?= /bin/cp

LIBS += -lc
FLAGS += --std=c++17 -fno-exceptions -g

all:
	mkdir -p bin 
	$(CXX) $(FLAGS) -o bin/vpktool $(wildcard src/*.cpp) $(wildcard src/*.c)

rebuild: clean all 

clean:
	rm -rf bin

install: all 
	$(CP) -vf bin/vpktool $(PREFIX)/bin/vpktool 
