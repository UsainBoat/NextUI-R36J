ifeq (,$(CROSS_COMPILE))
$(error missing CROSS_COMPILE for this toolchain)
endif

ifeq (,$(PLATFORM))
PLATFORM=$(UNION_PLATFORM)
endif

TARGET = keymon
PRODUCT = $(TARGET).elf

CC = $(CROSS_COMPILE)gcc
CFLAGS = -Os -I. -I../../all/common -I../platform/ -I$(PREFIX_LOCAL)/include -DPLATFORM=\"$(PLATFORM)\"
LDFLAGS = -L$(PREFIX_LOCAL)/lib -lmsettings -lpthread -lrt -ldl -Wl,--gc-sections -s

all:
	$(CC) $(TARGET).c -o $(PRODUCT) $(CFLAGS) $(LDFLAGS)

clean:
	rm -rf $(PRODUCT) *.o
