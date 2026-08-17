<!-- SPDX-License-Identifier: CC-BY-4.0 -->

# 再現・検証手順

この文書は研究上の検証条件を再現するためのチェックリストであり、未知の機器へ
そのまま Flash 書込みを行う手順ではない。公開版には OEM firmware、NAND dump、
Recovery image、個体依存の書込み runner を含めていない。

## Host 側

1. AArch64クロスコンパイラ、`dtc`、`make`を用意する。
2. U-Boot `527115ef6783cec49e5610c523c124b399011361`をcheckoutする。
3. [scripts/build-selector.sh](../scripts/build-selector.sh)へU-Boot treeと出力先を渡す。
4. scriptが公開source、integration patch、config fragment、ITSを適用してbuildする。
5. U-Boot tree内で生成した`tools/mkimage`でFITを作り、6 MiBへ`0xff` paddingする。
6. `mkimage -l`、DTB model / compatible、size、SHA-256を保存する。

```text
git clone https://github.com/u-boot/u-boot.git
git -C u-boot checkout 527115ef6783cec49e5610c523c124b399011361
./scripts/build-selector.sh ./u-boot ./out/selector
```

歴史上Flashしたartifactはdirty研究tree由来であるため、公開recipeの再buildが同じ
SHA-256になるとは限らない。機能再現と歴史artifact識別値を区別する。

## OpenWrt Image2

実機で動いた版はOpenWrt `r33051-f5dae5ece4`、Linux 6.12.94である。公開版には
OEM imageもOpenWrtのrootfs binaryも含めない。[src/openwrt](../src/openwrt/)に、
標準WSR-2533DHP2 DTSへ適用した最終Image2 partition / bootargs fragmentと、FIT/UBI
のassembly条件を収録する。

保持資料だけでは当時のpackage selectionを含むrootfsをbit-for-bit再buildできない。
再現可能なのは、同等OpenWrt kernel/rootfsを使った6 MiB FIT + 52 MiB UBIのlayout、
FIT pack、UBI volume geometryである。この限界は隠さず、歴史artifactのhashを
同一buildの保証に使わない。

## Linux RAM Recovery

[src/recovery](../src/recovery/)のDTS fragmentは、各工程でwritableにするpartitionを
一つの目的へ限定する。Recovery binaryと自動writerは配布しないが、次のinterlock
を再現できる形で残す。

- Image2-only: `image2-fit`と`image2-ubi`だけwritable
- Selector-only: `selector`だけwritable
- OEM Image1 restore: 58 MiBの`oem-image1-restore`だけwritable
- Config、factory、Image1/Image2の非対象側、glbcfg、board_dataをread-only
- `rdinit=/init`でRAM上に起動し、自動rebootしない

## RAM-only 検証

永続化前には、純正 U-Boot から Selector FIT を RAM へ転送し、RAM boot だけで
次を確認する。

```text
version
RAM上のSelector FITを iminfo で確認
bootm RAM_SELECTOR_ADDRESS
ROUTER: DHP2 -> aligned FIT -> OEM Linux
AP: raw FIT -> OpenWrt Linux -> UBI / overlay
WB: Console 停止、NAND read なし
```

長いコマンドは UART へ一括投入しない。転送は TFTP 等のネットワーク経路を使い、
UART は一 byte ずつ低速に送る。

## 受入れログ

ROUTER では次を確認する。

- GPIO 値と `EXPECT_DHP2`
- Image1 byte offset と page index
- DHP2 magic、inner FIT magic、FIT `totalsize`
- aligned RAM address からの bootm
- OEM Linux と `/dev/mtdblock7` 上の SquashFS mount

AP では次を確認する。

- GPIO 値と `EXPECT_RAW_FIT`
- Image2 byte offset と page index
- FIT hash、Linux version
- `ubi.mtd=image2-ubi`、UBI attach、rootfs / overlay mount

WB では次を確認する。

- GPIO 値
- Selector prompt への停止
- NAND read が発生していないこと

## 永続化を伴う実験を再実施する場合

対象 MTD の label、offset、size、erase size、write size、read-only 属性を同じ
Recovery 上で再確認する。対象 slot を一つに固定し、書込み前後の非対象 hash を
比較する。書込み後の自動reboot、`saveenv`、`dual_image_check`は行わない。ただし
完成構成を初期状態から作る際は、元Configをbackupしたうえで`boot2`を一度だけ
末尾Selector起動用へ永続変更する別工程が必要である。この危険工程は自動化して
いない。

この公開アーカイブでは安全上、永続 Flash 書込みを自動化するスクリプトを配布
しない。再現者は個体の boot chain、ECC、MTD driver、ライセンス済みの image の
利用条件を確認し、研究環境に合わせて手順を再設計する必要がある。
