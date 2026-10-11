# Host-side tests: builds the library against a small Arduino stand-in and runs the checks.
#   make -C test            build and run (C++17, as the library requires)
#   make -C test examples   also build and run every example

STD      ?= c++17
CXX      ?= g++
CXXFLAGS  = -std=$(STD) -Wall -Wextra -Wdouble-promotion -Wsign-compare -Wshadow -Werror \
            -DESP32 -Istub -I../src

SOURCES = ../src/JoystickUtilities.cpp stub/host.cpp

run: test_main
	./test_main

test_main: test_main.cpp $(SOURCES) ../src/JoystickUtilities.h stub/Arduino.h stub/Preferences.h
	$(CXX) $(CXXFLAGS) test_main.cpp $(SOURCES) -o $@

# Build every example against the stub and run it once
examples: $(SOURCES) stub/host_main.cpp
	@for dir in ../examples/*/; do \
	  name=$$(basename $$dir); \
	  $(CXX) $(CXXFLAGS) -include Arduino.h -x c++ $$dir$$name.ino -x none \
	    $(SOURCES) stub/host_main.cpp -o example_$$name || exit 1; \
	  ./example_$$name > example_$$name.log || exit 1; \
	  echo "$$name ok"; \
	done

clean:
	rm -f test_main example_*

.PHONY: run examples clean
