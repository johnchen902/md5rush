.PHONY: all clean

CXXFLAGS += -O3 -Wall -Wextra -Wshadow -std=c++17
LDFLAGS += -lcrypto -lpthread

all: md5rush

clean:
	$(RM) md5rush
