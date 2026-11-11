ifeq (,$(CROSS_COMPILE))
$(error missing CROSS_COMPILE for this toolchain)
endif
ifeq (,$(PREFIX))
$(error missing PREFIX for this toolchain)
endif

TARGET=msettings

.PHONY: all build clean

CC = $(CROSS_COMPILE)gcc

CFLAGS += -I. -I../../all/common -fpic -Ofast
LDFLAGS += -shared -lrt -lm -ldl

all: build

build:
	$(CC) -c -fpic "$(TARGET).c" $(CFLAGS)
	$(CC) -c -fpic "../../all/common/displaycal.c" $(CFLAGS)
	$(CC) -shared -o "lib$(TARGET).so" "$(TARGET).o" "displaycal.o" $(LDFLAGS)
	mkdir -p "$(PREFIX_LOCAL)/include"
	mkdir -p "$(PREFIX_LOCAL)/lib"
	cp "$(TARGET).h" "$(PREFIX_LOCAL)/include"
	cp "lib$(TARGET).so" "$(PREFIX_LOCAL)/lib"

clean:
	rm -f *.o "lib$(TARGET).so"
	rm -f "$(PREFIX_LOCAL)/include/$(TARGET).h"
	rm -f "$(PREFIX_LOCAL)/lib/lib$(TARGET).so"
