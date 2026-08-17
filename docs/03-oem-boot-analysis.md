<!-- SPDX-License-Identifier: CC-BY-4.0 -->

# OEM wrapper、bootargs、rootfs 経路

## DHP2 wrapper

Image1 の先頭には 28 bytes (`0x1c`) の DHP2 wrapper がある。先頭の little-endian
magic は表示上 `DHP2` と読め、header 内には image length field がある。観測値は
artifactによって異なった。

```text
Image1 slot       0x00200000
DHP2 header       0x1c bytes
inner FIT         slot + 0x1c
```

| artifact / 観測経路 | length field |
| --- | ---: |
| 58 MiB logical backup | `0x00600000` |
| 公式配布版1.28から復号したrecovery data | `0x00b12000` |

これらは 58 MiB slot 全体のサイズでも wrapper 自体のサイズでもない。
Selector はこれを NAND から読むべき総量として信用せず、整合性・上限チェックに
だけ使う。実際の read size は `slot + 0x1c` の FIT header から得る `totalsize` に
基づける。

## 純正 U-Boot の処理

調査開始時の主要environmentは次の構造だった。

```text
loadaddr=0x4007FF28
boot2=dual_image_check;if test ${dual_image} = good; then run boot_rd_img;bootm;fi
boot_rd_img=nand read ${loadaddr} 0x200000 2000;image_blks 2048;nand read ${loadaddr} 0x200000 ${img_align_size}
```

`dual_image_check`は純正二重化構成の検査・同期処理を含むため、OEM/OpenWrtを
意図的に異なる内容にした最終構成では実行しない。最終的にConfigの`boot2`
だけを次へ変更し、一度`saveenv`してreadbackした。

```text
boot2=nand read 0x41000000 0x7a00000 0x600000; iminfo 0x41000000; bootm 0x41000000
```

この変更後はConfigを保護し、Selector更新時には`saveenv`を実行していない。

純正 U-Boot では OEM Image1 の通常起動経路が DHP2 wrapper と結びついている。
wrapper 先頭をそのまま一般的な FIT の `bootm` 引数に渡すと、wrapper を考慮する
patched bootm の前提を満たす場合と、raw FIT として解釈する経路が一致しない場合
がある。調査では、次の二つを区別した。

- 純正 Image1 通常起動: wrapper / OEM 側の boot path が有効で、OEM Linux が rootfs
  まで起動する。
- inner FIT を 4-byte 境界の RAM address へ直接渡す試行: libfdt の alignment
  エラーや `Unknown image format` になった。

このため Selector は wrapper 先頭を `bootm` に渡さない。DHP2 magic と length を
検証し、`+0x1c` の FIT 本体を 8-byte 整列した RAM (`0x40080000`) へコピーして
通常の FIT として `bootm` へ渡す。

## bootargs と rootfs

初期の FIT/FDT 調査では `/chosen/bootargs` に early console、console、swiotlb
などしか見えず、`root=` の出所を FIT/FDT 単独に帰すことはできなかった。一方、
純正 Image1 の通常起動および最終 Selector 経路の Linux ログでは、Linux が
`/dev/mtdblock7` 上の SquashFS rootfs を root filesystem として mount した。

この差から、公開する結論は次のように整理する。

1. Selector は `bootargs`、FDT、rootfs 指定を注入・加工しない。
2. OEM FIT/FDT と純正 Image1 の DHP2/TRX 構造を保持する。
3. Linux 側の既存 parser と rootfs 検出経路に rootfs の認識を任せる。
4. 受入れ条件は command line に特定の `root=` が現れることではなく、
   `/dev/mtdblock7` 上の SquashFS mount が成功することである。

OEM の command line を別の OpenWrt image へ流用することや、Selector から
`root=` / `ubi.mtd=` を追加することは行わない。

## 直接起動で得た失敗と意味

過程では inner FIT を直接 `bootm` した Linux が、kernel と FDT の展開後に
`/dev/root: Can't open blockdev`、`unknown-block(0,0)` で停止した。この試行では
OEM Image1 の Linux 側 rootfs 経路と、別 slot の FIT / UBI 構成が一致していなかった。
従って、kernel/FDT が起動したことだけで rootfs 構造まで互換とは判断できない。

最終構成では OEM は Image1 全体の構造を保持し、OpenWrt は Image2 FIT と Image2
UBI を同じ構成として復元した。この分離によって、Selector は format dispatch
だけを担当できるようになった。
