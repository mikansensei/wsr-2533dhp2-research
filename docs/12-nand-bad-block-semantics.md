<!-- SPDX-License-Identifier: CC-BY-4.0 -->

# NAND bad block handling の現状と制約

## 要約

Hybrid Selector v3 は MT7622 NAND を **物理 page 番号で連続読出し**する
read-only 実装である。bad block を検出して後続の良好 block を詰めて読む処理、
BMT の参照、slot 内の remap は実装していない。

検証に用いた個体では、NAND 全域 1024 eraseblock の scan が read error 0、bad marker
0 だった。そのため、最終構成（Image1 OEM / Image2 OpenWrt / Selector）は当該個体で
成立した。この観測は、bad block を持つ個体でも固定 offset の slot を起動できることを
意味しない。

## 対象ごとの扱い

| 経路 | bad block に対する扱い | 確認状態 |
| --- | --- | --- |
| 純正 U-Boot の通常 `nand read` | U-Boot の skip-bad 経路を使う | normal read command として確認 |
| 純正 U-Boot の vendor BMT | binary に関連文字列はあるが、この機種・非ゼロbad blockでの経路は未検証 | 未確定 |
| OEM Linux の MTD/SquashFS | MTD の提供する address space を読む。SquashFS 自身は raw NAND の bad block を詰めない | generic Linux の動作として確認 |
| OpenWrt Image2 FIT の `mtd` write | MTD が bad block と報告する場合は skip-bad write を行う | tool の実装として確認 |
| OpenWrt Image2 UBI | UBI attach 時に bad physical eraseblock を管理対象から除外する | UBI の設計として確認 |
| Hybrid Selector v3 | 物理 page 連続read。skip / remap なし | 実装として確認 |

ここでいう MTD address space は、vendor の BMT 等が下層にある場合、必ずしも NAND
silicon の物理 offset と同義ではない。BMT がこの機種の各経路で実際に有効かどうかは、
bad block が存在する実機観測または対応する vendor source がない限り断定しない。

## Selector v3 が安全に扱える条件

次をすべて満たす個体だけを、現在の固定-offset Selector 構成の対象とする。

1. 対象slotを含む NAND の bad-block scan が 0 件である。
2. Selectorが読む範囲（Image1先頭から inner FIT、Image2 FIT、Selector自身）に
   uncorrectable ECC error がない。
3. FIT header の magic、totalsize、hash verification が通る。

いずれかが満たされない場合、SelectorはOSを推測して起動してはならない。現行の
fail-closed 方針どおり U-Boot Console で停止し、NAND layout と lower-layer mapping を
個別に調査する。

## 設計上の含意

page 内の OOB/FDM/ECC の扱いと、eraseblock 単位の bad-block translation は別問題で
ある。v3 は前者を読出しに必要な範囲で扱うが、後者を実装していない。

特に、Selector slot は NAND 末尾の 6 MiB にあり、後方に代替用の物理領域がない。
単純な skip-bad は read size を維持できず、一般的な U-Boot `nand read` の挙動を
そのまま持ち込んでも解決しない。remap/BMTを根拠付きで扱うか、slot配置そのものを
見直す必要がある。

## 将来の改善候補

最小の安全改善は、起動前に対象slot範囲を read-only scan し、bad marker または
uncorrectable ECC error を検出した時点で明示的にConsole停止することである。

bad block を持つ個体でも起動を成立させる変更は、BMTの実装・メタデータ・予備領域・
純正書込み経路との整合性を実機で検証してから別設計として扱う。固定 offset を単純に
skip する変更は、OEMのDHP2/TRX構造、OpenWrt FIT、UBIのいずれに対しても安全な一般解に
ならない。

## 運用上の注意

bad block 対応の検証は NAND write / erase や意図的なbad block作成を必要とし得る。
この公開アーカイブの手順は、それらの破壊的試験を推奨しない。検証前には対象slotの
read-only scan、readback、FIT検証を行い、異常時は自動repair、image copy、
`dual_image_check` を実行しない。
