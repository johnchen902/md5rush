.PHONY: all clean

LINK.o = $(LINK.cc)
CXXFLAGS += -O3 -Wall -Wextra -Wshadow -std=c++17 -march=native

all: md5rush-simd

md5rush-simd: md5rush-simd.cpp

clean:
	$(RM) md5rush-simd
