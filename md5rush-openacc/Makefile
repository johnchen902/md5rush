.PHONY: all clean

LINK.o = $(LINK.cc)
CXXFLAGS += -O3 -Wall -Wextra -Wshadow -std=c++17 -fopenacc

all: md5rush-openacc

md5rush-openacc: md5rush-openacc.cpp

clean:
	$(RM) md5rush-openacc
