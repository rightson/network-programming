RAS_ROOT := $(HOME)/ras
RAS_BIN_ROOT := $(RAS_ROOT)/bin
COMMANDS_LOC := ../commands
TEST_LOC := ../test
SPEC_LOC := $(PWD)/../spec

CXXFLAGS := -Wall -std=c++11 $(FLAG)

all: server

clean:
	$(RM) server 
	$(RM) $(RAS_ROOT)/test.html
	$(RM) $(RAS_BIN_ROOT)/ls
	$(RM) $(RAS_BIN_ROOT)/cat
	make -C $(COMMANDS_LOC) clean
	make -C $(TEST_LOC) ../test clean

install:
	make -C $(COMMANDS_LOC)
	cp -f $(SPEC_LOC)/test.html $(RAS_ROOT)/test.html
	cp -f /bin/ls $(RAS_BIN_ROOT)
	cp -f /bin/cat $(RAS_BIN_ROOT)
	ls -lat $(RAS_BIN_ROOT)

test: install
	make -C $(TEST_LOC) test

demo:
	make -C $(TEST_LOC) demo

.PHONY: all clean test demo

