KDIR ?= /lib/modules/$(shell uname -r)/build
CXX ?= g++
.PHONY: all module clean
all: module build/edu-test
build/edu-test: tools/edu-test.cpp include/edu_lab.h
	mkdir -p build
	$(CXX) -std=c++17 -O2 -Wall -Wextra -Werror -static -pthread -Iinclude $< -o $@
module:
	@bash scripts/build-module.sh "$(KDIR)"
clean:
	rm -rf build
	rm -f driver/*.ko
