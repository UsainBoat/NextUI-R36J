#!/bin/sh
# Build NextUI for R36J (Allwinner A33 / R36S Clone)
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

echo "1. Ensuring r36j-toolchain Docker image is built..."
if ! docker image inspect r36j-toolchain >/dev/null 2>&1; then
	docker build -t r36j-toolchain -f Dockerfile.r36j .
fi

echo "2. Building NextUI workspace for PLATFORM=r36j..."
docker run --rm -v "$SCRIPT_DIR/workspace:/root/workspace" r36j-toolchain bash -c "cd /root/workspace && make PLATFORM=r36j -j\$(nproc)"

echo "3. Assembling distribution folder in workspace/build/r36j/dist/nextui..."
DIST="$SCRIPT_DIR/workspace/build/r36j/dist/nextui"
mkdir -p "$DIST/bin" "$DIST/lib" "$DIST/cores" "$DIST/paks" "$DIST/res" "$DIST/shaders"

cp -r "$SCRIPT_DIR/skeleton/SYSTEM/r36j/bin/"* "$DIST/bin/" 2>/dev/null || true
cp -r "$SCRIPT_DIR/skeleton/SYSTEM/r36j/paks/"* "$DIST/paks/" 2>/dev/null || true
cp -r "$SCRIPT_DIR/skeleton/SYSTEM/r36j/shaders/"* "$DIST/shaders/" 2>/dev/null || true
cp -r "$SCRIPT_DIR/skeleton/SYSTEM/res/"* "$DIST/res/" 2>/dev/null || true
cp "$SCRIPT_DIR/skeleton/SYSTEM/r36j/system.cfg" "$DIST/system.cfg" 2>/dev/null || true
cp "$SCRIPT_DIR/skeleton/SYSTEM/r36j/start_nextui.sh" "$DIST/start_nextui.sh" 2>/dev/null || true

cp "$SCRIPT_DIR/workspace/all/"*/build/r36j/*.elf "$DIST/bin/"
cp "$SCRIPT_DIR/workspace/r36j/keymon/keymon.elf" "$DIST/bin/"
cp "$SCRIPT_DIR/workspace/build/r36j/usr/lib/"*.so "$DIST/lib/"
cp "$SCRIPT_DIR/workspace/all/minarch/build/r36j/"*.so* "$DIST/lib/" 2>/dev/null || true
cp "$SCRIPT_DIR/workspace/r36j/cores/output/"*.so "$DIST/cores/" 2>/dev/null || true

# Create real copies for shared library symlinks so adb push works cleanly
cd "$DIST/lib"
for base in libcrypto.so.3:libcrypto.so liblz4.so.1.9.4:liblz4.so.1 liblz4.so.1.9.4:liblz4.so libsamplerate.so.0.2.2:libsamplerate.so.0 libsamplerate.so.0.2.2:libsamplerate.so libzip.so.4.0:libzip.so.4 libzip.so.4.0:libzip.so; do
	src="${base%%:*}"
	dst="${base##*:}"
	if [ -f "$src" ]; then
		rm -f "$dst"
		cp "$src" "$dst"
	fi
done

echo "Build complete: $DIST"
