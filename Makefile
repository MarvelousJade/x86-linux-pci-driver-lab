KDIR ?= /lib/modules/$(shell uname -r)/build
CXX ?= g++
.PHONY: all module clean
all: module
module:
	@bash scripts/build-module.sh "$(KDIR)"
clean:
	rm -rf build
	rm -f driver/*.ko
