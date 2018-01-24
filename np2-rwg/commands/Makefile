CXX := g++
TARGETS := $(basename $(wildcard *.cpp))
RAS_ROOT := $(HOME)/ras
RAS_BIN_ROOT := $(HOME)/ras/bin

all: build install

build: $(TARGETS)

clean:
	$(RM) $(TARGETS)

install:
	if [ ! -d $(RAS_BIN_ROOT) ]; then mkdir -p $(RAS_BIN_ROOT); fi
	cp -f $(TARGETS) $(RAS_BIN_ROOT)
	cp -f /bin/ls $(RAS_BIN_ROOT)
	cp -f /bin/cat $(RAS_BIN_ROOT)

uninstall:
	$(RM) $(RAS_BIN_ROOT)/*

.PHONY: all build clean install uninstall

