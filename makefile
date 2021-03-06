IDIR=include
SDIR=src
ODIR=obj
TESTDIR=test

#CC=gcc
#CXX=g++

#CC=clang
#CXX=clang++

#GCOV=--coverage -lgcov

ASAN=-fsanitize=address -fno-omit-frame-pointer
MSAN=-fsanitize=memory -fsanitize-memory-track-origins  -fno-omit-frame-pointer -fPIE
TSAN=-fsanitize=thread

SANITIZER=

ARCH?=$(shell getconf LONG_BIT)
#DEBUG_RELEASE=-DDEBUG

CLIBS =-lm -pthread
CFLAGS= -m$(ARCH) -g -O2 -I$(IDIR) -I$(SDIR) $(GCOV) $(SANITIZER)

CXXFLAGS=$(CFLAGS) -std=c++1y  #-stdlib=libc++
CXXLIBS=-lgtest -lgtest_main $(CLIBS)

_DEPS=ncd.h bitset.h ncd_global.h
DEPS = $(patsubst %,$(IDIR)/%,$(_DEPS))

_OBJ=ncd_main.o ncd.o
OBJ=$(patsubst %,$(ODIR)/%,$(_OBJ))



all: ncd_main

$(ODIR)/%.o: $(SDIR)/%.c $(DEPS)
	$(CC) $(CFLAGS) -c -o $@ $< $(CLIBS)

ncd_main: $(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ) $(CLIBS)

unit_test: $(DEPS) $(OBJ) $(TESTDIR)/*.*pp
	$(CXX) obj/ncd.o $(TESTDIR)/*.cpp -o $@ $(CXXFLAGS) $(CXXLIBS)

.PHONY: clean

clean:
	rm $(ODIR)/*.o ncd_main unit_test
