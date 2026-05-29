# sparkd top-level Makefile

PREFIX ?= /opt/sparkd

.PHONY: all clean test daemon tools ui install

all: daemon tools

daemon:
	$(MAKE) -C daemon

tools:
	$(MAKE) -C daemon tools

test:
	$(MAKE) -C daemon test

ui:
	cd ui && npm run build

install: all tools ui
	install -d $(DESTDIR)$(PREFIX)/bin
	install -m 755 build/sparkd build/tools/spark-ui build/tools/spark-midi \
	  build/tools/spark-serial build/tools/sparkctl $(DESTDIR)$(PREFIX)/bin/
	install -d $(DESTDIR)$(PREFIX)/share/sparkd/ui
	cp -r ui/dist/* $(DESTDIR)$(PREFIX)/share/sparkd/ui/

clean:
	$(MAKE) -C daemon clean
