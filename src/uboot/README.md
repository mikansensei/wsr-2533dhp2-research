<!-- SPDX-License-Identifier: CC-BY-4.0 -->

# U-Boot 断片

このディレクトリには、研究中に変更した小規模な U-Boot 関連ファイルだけを収録
しています。完全な U-Boot tree、toolchain、board binary は含めません。

## ファイル

- `mt7622_nand_ro.c`: MT7622 raw NANDの物理page read-only prototypeと`wsrsel`
- `mt7622-buffalo-wsr-2533dhp2.dts`: UART / Ethernet / memory を含む host-side DTS
- `phase4m-selector-final.env`: 最終 Selector command の最小 environment fragment
- `phase4m-selector-dryrun.env`: GPIO read-only の dry-run fragment

ソースファイル自身の SPDX 表記が適用され、GPL-2.0-or-later で扱います。ライセンス
全文は [`../../LICENSES/GPL-2.0-or-later.txt`](../../LICENSES/GPL-2.0-or-later.txt) にあります。

## 出典と再利用

これらはローカルのU-Boot作業tree（上流
`527115ef6783cec49e5610c523c124b399011361`）で変更したファイルから抽出した。
統合差分は[`patches/u-boot`](../../patches/u-boot/)、configとITSは
[`configs`](../../configs/)、build recipeは
[`scripts/build-selector.sh`](../../scripts/build-selector.sh)に収録する。

`mt7622_nand_ro.c` は読み取り専用の検証段階を記録するもので、公開版に Flash
write / erase / repair の実装を追加していません。

readerはbad blockをskipしない。検証個体では全block scanがbad marker 0だったが、
これは別個体での固定offset読出しを保証しない。また、driver・診断command・機器
固有Selectorが一ファイルに同居する研究用PoCであり、upstream-ready driverではない。

このreaderが扱うpage内のECC/FDMと、eraseblock単位のbad-block translationは別の
層である。後者をv3が実装していないこと、BMTを仮定できないこと、non-zero bad block
での運用停止条件は[bad block handlingの技術ノート](../../docs/12-nand-bad-block-semantics.md)
を参照する。
