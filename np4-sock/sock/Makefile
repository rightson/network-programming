CC := $(CXX)
CXXFLAGS := -Wall -std=c++11 -g -I../lib
TARGET := sock
SOURCES := $(wildcard *.cpp) $(wildcard ../lib/*.cpp)
OBJECTS := $(SOURCES:.cpp=.o)

SOCK_BIN := ./sock
SOCK_PORT := 8000
SOCK_CONF := sock.conf
SOCK_CONF2 := clients_sock.conf

all: build runserver

build: $(TARGET)

$(TARGET): $(OBJECTS)

clean:
	$(RM) $(OBJECTS) $(DEPFILES) $(TARGET)

runserver:
	$(SOCK_BIN) $(SOCK_PORT) $(SOCK_CONF) $(SOCK_CONF2)

.PHONY: all build clean runserver
