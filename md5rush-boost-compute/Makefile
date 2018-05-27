.PHONY: all clean

LINK.o = $(LINK.cc)
CXXFLAGS += -O3 -Wall -Wextra -Wshadow -g -std=c++17
LDFLAGS += -lOpenCL

all: md5rush-boost-compute

md5rush-boost-compute: md5rush-boost-compute.cpp

clean:
	$(RM) md5rush-boost-compute
