<!-- SPDX-License-Identifier: CC-BY-4.0 -->

# Hybrid Selector の設計

## 固定 slot と expected format

今回は Image1=OEM、Image2=OpenWrt と固定したため、完全な自動判別だけに頼らず、
GPIO から決めた slot に期待形式を付与する。

```text
dispatch_selected_image(0x00200000, EXPECT_DHP2)
dispatch_selected_image(0x03c00000, EXPECT_RAW_FIT)
```

probe 結果が expected format と一致しなければ停止する。将来、slot の内容を入れ
替えて運用する必要が出た場合だけ `EXPECT_AUTO` を別モードとして追加する。

## 共通 dispatch の流れ

```text
read GPIO1 / GPIO16
  -> 既知の組み合わせなら slot と expected format を決める
  -> byte offset を NAND page index へ一度だけ変換
  -> stage buffer へ header を read
  -> expected format を検証
       DHP2: length sanity -> +0x1c の FIT header
       raw FIT: FIT magic -> totalsize
  -> FIT totalsize が slot 範囲内か検証
  -> FIT 全体を整列済み RAM へ read / copy
  -> FIT構造 / FDT を事前検証
  -> bootm が FIT subimage hash を検証
  -> bootm(aligned_fit_address)
```

WB はこの流れに入らず Console で停止する。未知 GPIO、magic 不一致、length /
totalsize の範囲外、NAND read error も停止する。

## DHP2 path

```text
flash offset       0x00200000
page index         0x0400
stage               0x41000000
inner FIT source    stage + 0x1c
aligned destination 0x40080000
bootm               0x40080000
```

DHP2 の inner FIT は slot 先頭から `0x1c` ずれるため、libfdt が要求する整列を
満たさないことがある。header probe は unaligned-safe な読み方にし、最終的な FIT
本体は整列済み RAM へコピーする。

`DHP2 length` は sanity 上限としてのみ使用し、実読込サイズは FIT `totalsize`
から決める。`0x1c + totalsize <= DHP2 length`、かつ slot 境界内であることを確認
する。

## raw FIT path

```text
flash offset       0x03c00000
page index         0x7800
stage               0x41000000
aligned destination 0x40080000
bootm               0x40080000
```

Selector は FIT magic、header size、`totalsize <= 6 MiB`、slot 境界を確認する。
その後 `bootm` が FIT 内の crc32 / sha1 hash nodeを確認する。Image2
FIT の FDT にある UBI / rootfs 情報をそのまま使用し、Selector は command line を
追加しない。

## v1 / v2 / v3 の差分

| 版 | 問題 | 改善 |
| --- | --- | --- |
| v1 | DHP2 の `+0x1c` をそのまま libfdt に渡し alignment error | probe と boot address の整列を分離 |
| v2 | `bootm 0x4007ff44` のような未整列 address で `Unknown image format` | FIT 本体を整列済み RAM へコピー |
| v3 | header / expected format / length / totalsize / slot bounds を確認 | common dispatch と安全停止を確定 |

## Selector の非責務

Selector は次の処理を持たない。

- Flash write / erase
- `saveenv`
- `dual_image_check`
- Image1/Image2 の同期、repair、fallback
- OEM 用または OpenWrt 用の bootargs 注入
- core router のネットワーク操作

これにより、起動選択の失敗が Flash の修復処理へ連鎖しない構成にした。

FIT hash は転送・保存時の破損検出であり、署名や Secure Boot ではない。第三者
による image 置換に対する真正性保証はこの構成の対象外である。
