.PHONY: all clean

LINK.o = $(LINK.cc)
CXXFLAGS += -O2 -Wall -Wextra -Wshadow -g -std=c++17
LDFLAGS += -lOpenCL

all: md5rush-opencl

md5rush-opencl: md5rush-opencl.cpp

clean:
	$(RM) md5rush-opencl
