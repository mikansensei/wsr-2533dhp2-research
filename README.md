# WSR-2533DHP2 research archive

Buffalo WSR-2533DHP2 を対象に、純正ブートチェーンを保持したまま
Image1 の OEM ファームウェア、Image2 の OpenWrt、物理スイッチで選択する
Boot Selector を組み合わせた検証記録です。

> ## 注意
>
> 本リポジトリは、OEM firmware と OpenWrt を本体のモード切り替えスイッチで選択するデュアルブート環境を、WSR-2533DHP2で実現することを目的に開発・検証した研究資産の公開場所です。現在の実機では、ROUTER=OEM、AP=OpenWrt、WB=Hybrid Selector Consoleのデュアルブート構成に成功しています。ただし、現時点ではCodexによる概要整理を経て公開しただけの段階であり、完全に完成された再現手順ではありません。NANDのBad Block位置など個体差によっては同じ構成を再現できない可能性があり、その他にも[既知の制約](docs/09-results.md)があります。最終的には、殻割りを必要とせずに再現できる手順とHybrid Selectorを完成させ、誰でも追試できる成果物として公開することを目指してます。ただし予定は未定です。

最終的な実機確認結果は次の構成です。

| スイッチ位置 | 起動先 |
| --- | --- |
| ROUTER | Image1: Buffalo 純正 OEM |
| AP | Image2: OpenWrt |
| WB | Hybrid Selector の U-Boot Console |

Selector は純正 Preloader、ATF、U-Boot を置き換えず、NAND 末尾の専用 6 MiB
領域から起動します。Image1 と Image2 の形式差を Selector のディスパッチ層で
吸収し、ROUTER では DHP2 wrapper、AP では raw FIT を検証してから共通の
`bootm` 経路へ渡します。未知の GPIO、形式不一致、範囲外の FIT、NAND 読出し
エラーは自動修復や fallback を行わず Console 停止とします。

純正 U-Boot から Selector へ到達するため、Config 領域に保存された `boot2` は
研究中に一度だけ Selector 起動用へ変更しています。純正ブートローダ本体を保持
したことと、environment の値まで未変更であることは区別してください。

## 読み方

- [プロジェクト概要](docs/00-project-overview.md)
- [機器と起動チェーンの分析](docs/01-device-analysis.md)
- [Flash layout と保護境界](docs/02-flash-layout.md)
- [OEM wrapper、bootargs、rootfs 経路](docs/03-oem-boot-analysis.md)
- [Image2 OpenWrt と UBI](docs/04-openwrt-image2.md)
- [Hybrid Selector の設計](docs/05-hybrid-selector.md)
- [調査・実装タイムライン](docs/06-timeline.md)
- [再現・検証手順](docs/07-reproduction.md)
- [安全な実験運用](docs/08-operation-safety.md)
- [最終結果と既知の制約](docs/09-results.md)
- [版・artifact・実験証拠](docs/10-versions-and-evidence.md)
- [OEM Image1復元経路](docs/11-oem-restore.md)
- [NAND bad block handling の現状と制約](docs/12-nand-bad-block-semantics.md)
- [ソースの来歴](PROVENANCE.md)

数値表は [data/](data/) に、公開対象とした U-Boot 断片、統合 patch、設定、
build recipe は [src/](src/) と [patches/](patches/) に置いています。検証ログの
サニタイズ済み抜粋は [evidence/](evidence/) にあります。OEM firmware、NAND dump、Recovery
image、完成済みの Flash 書込み artifact、大規模な第三者ソースツリーはこの
リポジトリには含めません。

## 重要な範囲

これは機器固有の観測と実装判断を記録した研究資料です。復旧用イメージの配布や、
未知の個体へそのまま適用するための書込みツールを目的としません。NAND の実効
レイアウト、ECC、bootloader の挙動は個体・版・書込み方式によって再確認が必要です。

## ライセンスと出典

- 本文書、表、擬似コード: CC BY 4.0
- build / 公開検査script: MIT
- `src/uboot/` の U-Boot 断片: GPL-2.0-or-later（各ファイルの SPDX 表記を優先）

詳細は [NOTICE.md](NOTICE.md) と [LICENSES/](LICENSES/) を参照してください。
