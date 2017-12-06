.PHONY: all clean

LINK.o = $(LINK.cc)
CXXFLAGS += -O3 -Wall -Wextra -Wshadow -std=c++17 -march=native
LDFLAGS += -lpthread -lboost_thread -lboost_system

all: md5rush md5sum

md5rush: md5rush.o md5.o
md5rush.o: md5rush.cpp md5.h amd64magic.h
md5.o: md5.cpp md5.h amd64magic.h

md5sum: CXXFLAGS += -DMD5_MAIN
md5sum: md5.cpp md5.h amd64magic.h
	$(LINK.cpp) $^ $(LOADLIBES) $(LDLIBS) -o $@

clean:
	$(RM) md5rush md5sum md5rush.o md5.o
