.PHONY: all clean

CXXFLAGS += -O3 -Wall -Wextra -Wshadow
LDFLAGS += -lcrypto

all: md5rush

clean:
	$(RM) md5rush
