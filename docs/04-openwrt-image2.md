<!-- SPDX-License-Identifier: CC-BY-4.0 -->

# Image2 OpenWrt と UBI

## Image2 の構成

Image2 は 6 MiB の FIT と、その後ろ 52 MiB の UBI で構成した。

```text
Image2 FIT: 0x03c00000 - 0x04200000
Image2 UBI: 0x04200000 - 0x07600000
```

FIT の FDT は Image2 UBI を指定し、rootfs は UBI の static volume、overlay は
dynamic volume として扱う。Selector はこの bootargs / FDT を変更しない。

## Linux NAND driver 経由の復元

Image2 UBI は raw 52 MiB blob の単純な byte-for-byte 書込みではなく、Linux
Recovery の NAND / UBI 経路で再構築した。

```text
ubiformat
  -> ubiattach
  -> rootfs static volume 作成
  -> rootfs_data dynamic volume 作成
  -> ubiupdatevol
  -> ubinfo と logical hash を確認
  -> ubidetach
```

観測した geometry は次の通り。

| 項目 | 値 |
| --- | ---: |
| PEB | 131072 bytes |
| LEB | 129024 bytes |
| min I/O | 2048 bytes |
| VID header offset | 512 bytes |
| UBI PEB 数 | 416 |
| rootfs | 41 LEB / static |
| rootfs_data | 349 LEB / dynamic |
| rootfs logical data | 5,224,448 bytes |

## 検証方法

UBI の erase counter、EC header、VID header は Linux 上の再構築時に変化し得る。
従って、再構築した raw 52 MiB 全体を元の参照 artifact と SHA-256 byte match
させることは要求しない。代わりに次を検証した。

- Image2 FIT の size と readback SHA-256
- `ubinfo` の PEB / LEB / volume type / volume size
- attach / detach の成功
- rootfs logical data の size と SHA-256
- OpenWrt Linux の UBI attach、rootfs、overlay mount

記録した参照値:

```text
Image2 FIT SHA-256:   d755f019fbe3cdb764b4e326488459b75046e5f249648cc0d59f63647c19976c
rootfs logical SHA:   412e70ea06412ed5a5db68d6bfc1f5d693a5d7080b4268381e60816136f686f0
```

実機ログはOpenWrt `r33051-f5dae5ece4`、Linux 6.12.94を示した。完全な当時の
package configは保持されていないため、公開資材からrootfsのbit-for-bit再buildが
できるとは主張しない。layoutとUBI assemblyの再現範囲は
[`src/openwrt`](../src/openwrt/)に記録する。

## Image1 との分離

Image1 の OEM rootfs と Image2 の OpenWrt rootfs は共有しない。Image1 は OEM の
DHP2/TRX parser と rootfs 検出経路、Image2 は FIT が指定する UBI と volume を
使う。異なる Linux / DTB / rootfs の組み合わせを一つの bootargs で動かそうと
しないことが、今回の切り分けで得た重要な知見だった。
