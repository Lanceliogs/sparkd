# sparkd top-level Makefile

.PHONY: all clean test daemon sparkctl ui

all: daemon sparkctl

daemon:
	$(MAKE) -C daemon

sparkctl:
	$(MAKE) -C tools

test:
	$(MAKE) -C daemon test

ui:
	cd ui && npm run build

clean:
	$(MAKE) -C daemon clean
	$(MAKE) -C tools clean
