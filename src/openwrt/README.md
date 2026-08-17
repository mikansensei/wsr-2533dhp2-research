<!-- SPDX-License-Identifier: CC-BY-4.0 -->

# OpenWrt Image2 assembly notes

実機で受入れたImage2はOpenWrt `r33051-f5dae5ece4`、Linux 6.12.94を基にした。
標準のWSR-2533DHP2 supportを出発点とし、FITのDTBだけを
[image2-layout.dtsi](image2-layout.dtsi)相当のpartition / bootargsへ変更した。

保持資料には当時の完全なOpenWrt `.config`とpackage feed lockがないため、rootfsを
bit-for-bit再buildできるとは主張しない。公開する再現境界は次である。

- Image2 FIT: 6 MiB、raw FIT、kernel/FDTはcrc32とsha1 nodeを持つ
- Image2 UBI: 52 MiB、PEB 131072、LEB 129024、min I/O 2048、VID offset 512
- `rootfs`: static、41 LEB、logical data 5224448 bytes
- `rootfs_data`: dynamic、349 LEB
- bootargs: `ubi.mtd=image2-ubi root=/dev/ubiblock0_0 rootfstype=squashfs rootwait`

UBIは`ubiformat`、`ubiattach`、`ubimkvol`、`ubiupdatevol`で実機上に再構築した。
raw 52 MiB artifactのSHA一致は受入れ条件ではない。
