#!/bin/sh
# SPDX-License-Identifier: MIT
set -eu

BASE=527115ef6783cec49e5610c523c124b399011361
SELF_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
REPO_DIR=$(dirname "$SELF_DIR")
UBOOT_DIR=${1:?usage: build-selector.sh UBOOT_DIR OUTPUT_DIR}
OUT_DIR=${2:?usage: build-selector.sh UBOOT_DIR OUTPUT_DIR}
CROSS_COMPILE=${CROSS_COMPILE:-aarch64-linux-gnu-}
JOBS=${JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 1)}

test "$(git -C "$UBOOT_DIR" rev-parse HEAD)" = "$BASE" || {
	echo "error: U-Boot must be at $BASE" >&2
	exit 1
}
test -z "$(git -C "$UBOOT_DIR" status --porcelain)" || {
	echo "error: use a clean U-Boot checkout" >&2
	exit 1
}

OUT_DIR=$(mkdir -p "$OUT_DIR" && CDPATH= cd -- "$OUT_DIR" && pwd)
git -C "$UBOOT_DIR" apply --check "$REPO_DIR/patches/u-boot/0001-wsr2533-selector-integration.patch"
git -C "$UBOOT_DIR" apply "$REPO_DIR/patches/u-boot/0001-wsr2533-selector-integration.patch"
install -m 0644 "$REPO_DIR/src/uboot/mt7622_nand_ro.c" \
	"$UBOOT_DIR/drivers/mtd/nand/raw/mt7622_nand_ro.c"
install -m 0644 "$REPO_DIR/src/uboot/mt7622-buffalo-wsr-2533dhp2.dts" \
	"$UBOOT_DIR/arch/arm/dts/mt7622-buffalo-wsr-2533dhp2.dts"
install -m 0644 "$REPO_DIR/src/uboot/phase4m-selector-final.env" \
	"$UBOOT_DIR/board/mediatek/mt7622/phase4m-selector-final.env"

make -C "$UBOOT_DIR" O="$OUT_DIR" mt7622_rfb_defconfig
"$UBOOT_DIR/scripts/kconfig/merge_config.sh" -m -O "$OUT_DIR" \
	"$OUT_DIR/.config" "$REPO_DIR/configs/selector-final.fragment"
make -C "$UBOOT_DIR" O="$OUT_DIR" olddefconfig
make -C "$UBOOT_DIR" O="$OUT_DIR" CROSS_COMPILE="$CROSS_COMPILE" -j"$JOBS"

install -m 0644 "$REPO_DIR/configs/selector-final.its" "$OUT_DIR/selector-final.its"
(cd "$OUT_DIR" && ./tools/mkimage -f selector-final.its selector-final.itb)
dd if=/dev/zero bs=1048576 count=6 2>/dev/null | tr '\000' '\377' \
	> "$OUT_DIR/selector-final-6MiB.padded.bin"
dd if="$OUT_DIR/selector-final.itb" of="$OUT_DIR/selector-final-6MiB.padded.bin" \
	conv=notrunc 2>/dev/null

test "$(wc -c < "$OUT_DIR/selector-final-6MiB.padded.bin")" -eq 6291456
(cd "$OUT_DIR" && ./tools/mkimage -l selector-final.itb && \
	sha256sum u-boot.bin u-boot.dtb selector-final.itb selector-final-6MiB.padded.bin)
