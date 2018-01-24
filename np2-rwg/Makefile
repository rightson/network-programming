RAS_ROOT := $(HOME)/ras
RAS_BIN_ROOT := $(RAS_ROOT)/bin
COMMANDS_ROOT := commands
TEST_ROOT := test
SPEC_ROOT := spec
LIB_SOURCES := $(wildcard lib/*.cpp)
LIB_OBJECTS := $(filter %o,$(patsubst %.cpp, %.o, $(LIB_SOURCES)))
IPC_ROOT := $(PWD)/ipc

LOC := -DRAS_ROOT="\"$(RAS_ROOT)\"" -DIPC_ROOT="\"$(IPC_ROOT)\""
CXXFLAGS := -Wall -std=c++11 $(LOC) $(FLAG)

ifneq ($filter demo,$(MAKECMDGOALS),)
CXXFLAGS += -DDEMO="1"
endif

all: build

build: rwg-shm rwg-spc

rwg-shm: rwg-shm.cpp $(LIB_OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $< $(LIB_OBJECTS)
	@mkdir -p $(IPC_ROOT)

rwg-spc: rwg-spc.cpp $(LIB_OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $< $(LIB_OBJECTS)
	@mkdir -p $(IPC_ROOT)

lib/%.o: lib/%.cpp lib/%.hpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

demo: rwg-shm
	@echo "This build is for demo"

clean:
	$(RM) lib/*.o
	$(RM) rwg-shm
	$(RM) $(IPC_ROOT)/fifo.*

install:
	make -C $(COMMANDS_ROOT)
	make -C $(SPEC_ROOT)
	ls -la $(RAS_ROOT) $(RAS_BIN_ROOT)

uninstall:
	make -C $(COMMANDS_ROOT) uninstall
	make -C $(SPEC_ROOT) uninstall
	ls -la $(RAS_ROOT) $(RAS_BIN_ROOT)

test: install
	make -C $(TEST_ROOT) clean test

.PHONY: all build clean install uninstall test demo

