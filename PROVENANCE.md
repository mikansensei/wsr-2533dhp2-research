<!-- SPDX-License-Identifier: CC-BY-4.0 -->

# Source provenance

## U-Boot

- Upstream: <https://github.com/u-boot/u-boot>
- Base commit: `527115ef6783cec49e5610c523c124b399011361`
- Project additions: WSR DTS, environment fragments, MT7622 read-only NAND
  adaptation, `wsrsel`, and fixed-link chainload integration
- No OEM U-Boot source or decompiled OEM function body is included.

## Linux references used by the NAND adaptation

- Tag: Linux v6.12
- `drivers/mtd/nand/raw/mtk_nand.c`
- `drivers/mtd/nand/ecc-mtk.c`
- Upstream: <https://github.com/torvalds/linux/tree/v6.12/drivers/mtd/nand>

The two referenced files identify MediaTek Inc. as copyright holder, Xiaolei
Li and Jorge Ramirez-Ortiz as authors, and use `GPL-2.0 OR MIT`. The adaptation
retains this notice, selects the MIT option for definitions/logic derived from
those files, and distributes the combined U-Boot adaptation under
GPL-2.0-or-later.

Register constants and controller sequencing were adapted to the observed
2 KiB page / 64-byte OOB / 4-bit ECC device. GPIO policy, fixed slot layout,
DHP2 checks, FIT dispatch, bounds checks, and console-stop behavior are new
project logic.

## OpenWrt and DTS

- Upstream: <https://github.com/openwrt/openwrt>
- Recorded running build: `r33051-f5dae5ece4`, Linux 6.12.94
- Published OpenWrt/Recovery files are layout fragments and observations;
  neither an OpenWrt source tree nor a built image is copied here.
- U-Boot DTS was written for this chainload experiment using upstream MT7622
  DTSI interfaces. It does not copy an OEM DTS.

## OEM material

No Buffalo firmware, decrypted recovery data, NAND dump, bootloader dump,
calibration data, or reverse-engineered OEM source is included. OEM names,
magic values, offsets, metadata and short console observations are recorded
only to describe interoperability. Users obtain any firmware they are
licensed to use from the vendor.
