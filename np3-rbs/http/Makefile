DEBUG := 0

CC := $(CXX)
CXXFLAGS := -Wall -std=c++11 -DDEBUG=$(DEBUG) $(gdb) -I../lib
OUTPUT_OPTION = -MMD -MP -o $@
TARGET := http
SOURCES := $(wildcard *.cpp) $(wildcard ../lib/*.cpp)
OBJECTS := $(SOURCES:.cpp=.o)
DEPFILES := $(SOURCES:.cpp=.d)

all: $(TARGET)

$(TARGET): $(OBJECTS)

clean:
	$(RM) $(OBJECTS) $(DEPFILES) $(TARGET)

-include $(DEPFILES)

.PHONY: clean
