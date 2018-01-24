SHELL := /bin/bash
PORT := 7000
DIFF := diff --strip-trailing-cr 

client:

clean:
	$(RM) ~/ras/*.txt ~/ras/setenv ~/ras/number

test: client clean
	$(call testRunner,1,6,.)

define testRunner
	@for i in {$1..$2}; do \
		infile=$3/test/test$${i}.txt; \
		outfile=$3/output/output$${i}.txt; \
		echo "Testing $${infile} and comparing result with $${outfile}"; \
		./client localhost $(PORT) $${infile} | $(DIFF) $${outfile} -; \
		status=$$?; \
		if [ $${status} -eq 0 ]; then echo "=> Pass"; else echo "=> Failed"; fi; \
	done;
endef

.PHONY: test clean demo
