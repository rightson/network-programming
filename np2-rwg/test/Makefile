SHELL := /bin/bash
PORT := 7001
DIFF := diff --strip-trailing-cr 
RAS_ROOT := $(HOME)/ras
TARGET := delayclient

all: build test

build: $(TARGET)

clean: clean-build clean-test

clean-build:
	$(RM) $(TARGET)

clean-test:
	-find $(RAS_ROOT)/* -maxdepth 0 -not -name 'test.html' -type f -exec rm {} \;

test: clean-test
	$(call testRunner,1,4,.)

define testRunner
	for i in {$1..$2}; do \
		./$(TARGET) localhost $(PORT) $3/test/test$${i}.txt | tee out$${i} | $(DIFF) $3/ans/ans$${i} -; \
		status=$$?; \
		if [ $${status} -eq 0 ]; then echo "=> Pass"; else echo "=> Failed"; fi; \
	done;
endef

.PHONY: all build test clean
