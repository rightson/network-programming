CC := $(CXX)
CXXFLAGS := -Wall -std=c++11 -g -I../lib
TARGET := httpd
SOURCES := $(wildcard *.cpp) $(wildcard ../lib/*.cpp)
OBJECTS := $(SOURCES:.cpp=.o)

-include ../setting.mk

HTTPD_BIN := ./$(HTTPD_ROOT)/httpd
HTTPD_PORT := 8000
PORT := $(HTTPD_PORT)

all: build runserver

build: $(TARGET)

$(TARGET): $(OBJECTS)

clean:
	$(RM) $(OBJECTS) $(DEPFILES) $(TARGET)

runserver:
	mkdir -p $(HTTPD_DOC_ROOT)
	$(HTTPD_BIN) $(PORT) $(HTTPD_DOC_ROOT)

.PHONY: all build clean runserver
