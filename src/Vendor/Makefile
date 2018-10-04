SUBDIRS=bdwgc

all: $(SUBDIRS)

bdwgc/configure:
	cd bdwgc; \
	$(SHELL) autogen.sh

bdwgc/Makefile:
	cd bdwgc; \
	$(SHELL) configure

bdwgc: bdwgc/configure bdwgc/Makefile
	$(MAKE) -C $@ extra/gc.o

.PHONY: $(SUBDIRS)
