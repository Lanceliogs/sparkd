# sparkd top-level Makefile

PREFIX  ?= /opt/sparkd
VERSION := $(shell sed -n 's/.*SPARKD_VERSION "\(.*\)"/\1/p' daemon/src/consts.h)
DIST_NAME := sparkd-v$(VERSION)-linux-x64

BINS := build/sparkd build/tools/sparkctl build/tools/spark-ui \
        build/tools/spark-midi build/tools/spark-serial build/tools/spark-reaper

.PHONY: all clean test daemon tools ui ui-rebuild ui-fetch ui-clean install uninstall installer dist deb

all: daemon tools

daemon:
	$(MAKE) -C daemon

tools: ui
	$(MAKE) -C daemon tools

test:
	$(MAKE) -C daemon test

ui:
	@if [ ! -d ui/dist ]; then echo "[ui] Building..."; cd ui && npm run build; \
	else echo "[ui] ui/dist/ exists, skipping (use 'make ui-rebuild' to force)"; fi

ui-rebuild:
	cd ui && npm run build

ui-fetch:
	@echo "[ui] Downloading pre-built UI for v$(VERSION)..."
	curl -fsSL https://github.com/music-music/sparkd/releases/download/v$(VERSION)/ui-dist.tar.gz | tar xz -C ui/
	@echo "[ui] ui/dist/ ready"

ui-clean:
	rm -rf ui/dist

install: all tools ui
	install -d $(DESTDIR)$(PREFIX)/bin
	install -m 755 $(BINS) $(DESTDIR)$(PREFIX)/bin/
	install -d $(DESTDIR)$(PREFIX)/ui
	cp -r ui/dist/* $(DESTDIR)$(PREFIX)/ui/
	install -d $(DESTDIR)/usr/local/bin
	@for bin in $(notdir $(BINS)); do \
	    ln -sf $(PREFIX)/bin/$$bin $(DESTDIR)/usr/local/bin/$$bin; \
	done
	@echo ""
	@echo "  sparkd installed to $(PREFIX)"
	@echo "  Binaries symlinked to /usr/local/bin"
	@echo "  Run: sparkctl --help"
	@echo ""

uninstall:
	@for bin in $(notdir $(BINS)); do \
	    rm -f $(DESTDIR)/usr/local/bin/$$bin; \
	done
	rm -rf $(DESTDIR)$(PREFIX)
	@echo "  sparkd uninstalled"

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
