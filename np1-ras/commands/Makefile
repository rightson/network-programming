CXX := g++
TARGETS := $(basename $(wildcard *.cpp))
RAS_BIN_LOC := $(HOME)/ras/bin

all: build install

build: $(TARGETS)

install:
	if [ ! -d $(RAS_BIN_LOC) ]; then mkdir -p $(RAS_BIN_LOC); fi
	cp -f $(TARGETS) $(RAS_BIN_LOC)

clean:
	$(RM) $(TARGETS)

.PHONY: all build install clean

