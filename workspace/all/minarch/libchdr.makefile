# Makefile wrapper for libchdr
ifeq (,$(PLATFORM))
	$(error please specify PLATFORM, eg. PLATFORM=tg5040 make)
endif

BUILD_DIR = build/$(PLATFORM)

.PHONY: all build clean install

ifeq ($(PLATFORM), r36j)
all: build install

build: $(BUILD_DIR)/libchdr.so

$(BUILD_DIR)/libchdr.so: | $(BUILD_DIR)
	$(CROSS_COMPILE)gcc -O3 -fPIC -shared -Iinclude -Ideps/lzma-26.02/include -Ideps/miniz-3.1.2 \
		src/libchdr_bitstream.c src/libchdr_cdrom.c src/libchdr_chd.c \
		src/libchdr_codec_avhuff.c src/libchdr_codec_cdfl.c src/libchdr_codec_cdlz.c \
		src/libchdr_codec_cdzl.c src/libchdr_codec_cdzs.c src/libchdr_codec_flac.c \
		src/libchdr_codec_huff.c src/libchdr_codec_lzma.c src/libchdr_codec_zlib.c \
		src/libchdr_codec_zstd.c src/libchdr_flac.c src/libchdr_huffman.c \
		deps/lzma-26.02/src/LzmaDec.c deps/miniz-3.1.2/miniz.c \
		-o $(BUILD_DIR)/libchdr.so -lz

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

install: $(BUILD_DIR)/libchdr.so
	mkdir -p "$(PREFIX_LOCAL)/lib"
	cp $(BUILD_DIR)/libchdr.so "$(PREFIX_LOCAL)/lib/"
	mkdir -p "$(PREFIX_LOCAL)/include/libchdr"
	cp -r include/libchdr/* "$(PREFIX_LOCAL)/include/libchdr/"

clean:
	rm -rf $(BUILD_DIR)
else
# Cross-compilation settings (only for non-desktop platforms)
ifneq ($(PLATFORM),desktop)
CMAKE_EXTRA = -DCMAKE_TOOLCHAIN_FILE=$(CMAKE_TOOLCHAIN_FILE)
endif

all: build install

build: $(BUILD_DIR)/libchdr.so

$(BUILD_DIR)/libchdr.so: | $(BUILD_DIR)
	cd $(BUILD_DIR) && cmake ../.. \
		-DBUILD_SHARED_LIBS=ON \
		-DINSTALL_STATIC_LIBS=OFF \
		-DCMAKE_BUILD_TYPE=Release \
		-DWITH_SYSTEM_ZLIB=OFF \
		-DCMAKE_SHARED_LIBRARY_SUFFIX_C=".so" \
		$(CMAKE_EXTRA)
	cd $(BUILD_DIR) && make -j$$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
	@if [ -f $(BUILD_DIR)/libchdr.so ]; then \
		echo "libchdr.so built successfully"; \
	elif [ -f $(BUILD_DIR)/src/libchdr.so ]; then \
		cp $(BUILD_DIR)/src/libchdr.so $(BUILD_DIR)/; \
	fi

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

install: $(BUILD_DIR)/libchdr.so
	mkdir -p "$(PREFIX_LOCAL)/lib"
	cp $(BUILD_DIR)/libchdr.so "$(PREFIX_LOCAL)/lib/"
	mkdir -p "$(PREFIX_LOCAL)/include/libchdr"
	cp -r include/libchdr/* "$(PREFIX_LOCAL)/include/libchdr/"

clean:
	rm -rf $(BUILD_DIR)
endif
