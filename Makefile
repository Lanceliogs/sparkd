# sparkd top-level Makefile

.PHONY: all clean test daemon tools ui

all: daemon tools

daemon:
	$(MAKE) -C daemon

tools:
	$(MAKE) -C daemon tools

test:
	$(MAKE) -C daemon test

ui:
	cd ui && npm run build

clean:
	$(MAKE) -C daemon clean
