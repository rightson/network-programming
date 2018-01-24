CC := $(CXX)
CXXFLAGS := -Wall -std=c++11 -g -I../lib
SOURCES := $(wildcard *.cpp) $(wildcard ../lib/*.cpp)
OBJECTS := $(SOURCES:.cpp=.o)
TARGET := cgi

-include ../setting.mk

all: build install

build: $(TARGET)

$(TARGET): $(OBJECTS)

clean:
	$(RM) $(OBJECTS) $(DEPFILES) $(TARGET)

install:
	cp -f cgi $(HTTPD_DOC_ROOT)/hw4.cgi
	ls -l $(HTTPD_DOC_ROOT)

uninstall:
	$(RM) $(HTTPD_DOC_ROOT)/hw4.cgi
	ls -l $(HTTPD_DOC_ROOT)

.PHONY: all build clean install uninstall

