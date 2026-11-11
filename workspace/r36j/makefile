ifeq (,$(PLATFORM))
PLATFORM=$(UNION_PLATFORM)
endif

ifeq (,$(PLATFORM))
$(error please specify PLATFORM, eg. PLATFORM=r36j make)
endif

all:
	cd libmsettings && make
	cd keymon && make

early:
	@echo "early build for r36j: done"

clean:
	cd libmsettings && make clean
	cd keymon && make clean
