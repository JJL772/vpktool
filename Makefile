
PREFIX ?= /usr/local/
CC ?= gcc
CXX ?= g++
CP ?= /bin/cp

LIBS += -lc
FLAGS += --std=c++17 -fno-exceptions -g

SRCS = src/vpk.cpp src/vpk1.cpp src/vpk2.cpp src/vpktool.cpp src/wad.cpp

all:
	mkdir -p bin 
	$(CXX) $(FLAGS) -o bin/vpktool $(SRCS)

rebuild: clean all 

clean:
	rm -rf bin

install: all 
	$(CP) -vf bin/vpktool $(PREFIX)/bin/vpktool 
