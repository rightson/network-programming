DEBUG := 0

CC := $(CXX)
CXXFLAGS := -Wall -std=c++11 -DDEBUG=$(DEBUG) $(gdb) -I../lib
OUTPUT_OPTION = -MMD -MP -o $@
TARGET := cgi
SOURCES := $(wildcard *.cpp) $(wildcard ../lib/*.cpp)
OBJECTS := $(SOURCES:.cpp=.o)
DEPFILES := $(SOURCES:.cpp=.d)

SERVER_ROOT := ~/public_html

export QUERY_STRING="h1=localhost&p1=7001&f1=t1.txt&h2=localhost&p2=7002&f2=t2.txt&h3=localhost&p3=7003&f3=t3.txt&h4=localhost&p4=7004&f4=t4.txt&h5=localhost&p5=7005&f5=t5.txt&h6=localhost&p6=7006&f6=t6.txt"

all: $(TARGET)

$(TARGET): $(OBJECTS)

install:
	cp -f cgi $(SERVER_ROOT)/hw3.cgi

uninstall:
	$(RM) $(SERVER_ROOT)/hw3.cgi

qs:
	@echo $(QUERY_STRING)

test-cgi:
	./cgi

clean:
	$(RM) $(OBJECTS) $(DEPFILES) $(TARGET)

-include $(DEPFILES)

.PHONY: clean
