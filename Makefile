# sparkd top-level Makefile

PREFIX  ?= /opt/sparkd
VERSION := $(shell sed -n 's/.*SPARKD_VERSION "\(.*\)"/\1/p' daemon/src/consts.h)
DIST_NAME := sparkd-v$(VERSION)-linux-x64

BINS := build/sparkd build/tools/sparkctl build/tools/spark-ui \
        build/tools/spark-midi build/tools/spark-serial build/tools/spark-reaper

.PHONY: all clean test daemon tools ui install installer dist deb

all: daemon tools

daemon:
	$(MAKE) -C daemon

tools: ui
	$(MAKE) -C daemon tools

test:
	$(MAKE) -C daemon test

ui:
	cd ui && npm run build

install: all tools ui
	install -d $(DESTDIR)$(PREFIX)/bin
	install -m 755 $(BINS) $(DESTDIR)$(PREFIX)/bin/
	install -d $(DESTDIR)$(PREFIX)/ui
	cp -r ui/dist/* $(DESTDIR)$(PREFIX)/ui/

# --- Tarball ---

dist: all tools ui
	rm -rf build/$(DIST_NAME)
	mkdir -p build/$(DIST_NAME)/bin
	cp $(BINS) build/$(DIST_NAME)/bin/
	mkdir -p build/$(DIST_NAME)/ui
	cp -r ui/dist/* build/$(DIST_NAME)/ui/
	cp pkg/install.sh build/$(DIST_NAME)/
	chmod +x build/$(DIST_NAME)/install.sh
	tar -czf build/$(DIST_NAME).tar.gz -C build $(DIST_NAME)
	@echo "\n  Created: build/$(DIST_NAME).tar.gz\n"

# --- Debian package ---

deb: all tools ui
	rm -rf /tmp/sparkd-deb
	$(MAKE) install DESTDIR=/tmp/sparkd-deb PREFIX=/opt/sparkd
	install -d -m 0755 /tmp/sparkd-deb/DEBIAN
	VERSION=$(VERSION) envsubst < pkg/deb/control > /tmp/sparkd-deb/DEBIAN/control
	install -m 0755 pkg/deb/postinst /tmp/sparkd-deb/DEBIAN/postinst
	install -m 0755 pkg/deb/prerm /tmp/sparkd-deb/DEBIAN/prerm
	dpkg-deb --build /tmp/sparkd-deb build/sparkd_$(VERSION)_amd64.deb
	rm -rf /tmp/sparkd-deb
	@echo "\n  Created: build/sparkd_$(VERSION)_amd64.deb\n"

# --- Windows installer ---

installer: all tools
	$(MAKE) -C daemon installer

clean:
	$(MAKE) -C daemon clean
