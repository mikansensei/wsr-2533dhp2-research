<!-- SPDX-License-Identifier: CC-BY-4.0 -->

# プロジェクト概要

## 研究目的

WSR-2533DHP2 の既存ブートチェーンを調べ、純正 U-Boot を維持したまま、物理
スイッチで次の三つを選べる構成を作ることを目的とした。

1. ROUTER: Buffalo 純正 OEM firmware
2. AP: Image2 に配置した OpenWrt
3. WB: 調査用 Boot Selector U-Boot Console

研究上の中心課題は、同じ NAND 上にある二つの slot が異なる形式である点だった。
Image1 は DHP2 wrapper の後ろに OEM FIT と rootfs を持ち、Image2 は raw FIT と
別の UBI を持つ。Selector はこの差を検出・検証し、OS ごとの bootargs を勝手に
変更せず、各 image 自身の起動情報を尊重する設計とした。

## 最終アーキテクチャ

```text
BootROM
  -> 純正 Preloader
  -> 純正 ATF
  -> 純正 U-Boot 2014.04-rc1
  -> NAND末尾の Hybrid Selector
       -> GPIO1/GPIO16 を読む
       -> ROUTER: Image1 を DHP2 として検証
       -> AP: Image2 を raw FIT として検証
       -> WB: Selector Console で停止
```

純正 Preloader、ATF、U-Boot 本体、factory、board data は保持した。Config 領域
そのものは保持したが、純正 U-Boot から Selector へ到達させるため、研究中に
`boot2` だけを一度永続変更した。以後の Hybrid Selector 更新工程では Config を
read-only とし、`saveenv` を禁止した。Selector
は read / validate / boot のみを担当し、erase、write、repair、fallback、
`dual_image_check`、`saveenv` を持たない。

## 到達した結果

Selector v3 は RAM-only 検証後、専用の 6 MiB 領域へ反映された。Selector 自身の
readback と `mtd verify` は成功し、書込み前後で非対象領域の hash が変化しない
ことを確認した。最後に三つのスイッチ位置を実機で確認し、ROUTER=OEM、AP=OpenWrt、
WB=Console の分岐を受け入れた。

## 記録の読み方

本アーカイブの数値は「機器上で観測した値」「構造から導いた値」「実装上の安全
条件」をできるだけ分けて記述する。OEM image の内部ファイルを再配布するのでは
なく、先頭 magic、offset、FIT metadata、Linux の mount 結果など、研究に必要な
最小限の観測結果を扱う。
