<!-- SPDX-License-Identifier: CC-BY-4.0 -->

# 最終結果と既知の制約

## 実機受入れ結果

ユーザーによる最後の三ポジション試験で、次を確認した。

| 条件 | 結果 |
| --- | --- |
| ROUTER | Image1 の Buffalo 純正 OEM が起動 |
| AP | Image2 の OpenWrt が起動 |
| WB | Hybrid Selector の U-Boot Console で停止 |

純正 Preloader、ATF、U-Boot を維持したまま、Image1 と Image2 を異なる形式の
firmware として並存させる最終目的を達成した。

## Selector artifact の識別値

Selector v3 の公開識別値は次の通り。

```text
6 MiB padded Selector: 4b0197a458749b772554929867c472d2f3eeb887438bd8a0246f38b87f28348f
U-Boot payload:        bdb84cb6970ee2a02425690a7440ad97d14f8b10d105476cd7fc3d9481755ff5
FIT:                   82b9f6f8ba30734a054b496a8c8316fd562e2542b1b34d69bff6feb57abe095e
DTB:                   99908b8ddf3fabc8c59c6622e6865d23e002e1f126f9288a1e9cd66dc42fc771
```

Image2 FIT と rootfs logical data の参照値は [Image2 の記録](04-openwrt-image2.md)
に示した。raw UBI 全体の hash は Linux 再構築の性質上、論理内容の同一性を
示す値として扱わない。

## 実装上の重要な結果

- DHP2 wrapper 自体は 28 bytes であり、header length field と区別する必要がある。
- DHP2 inner FIT は `+0x1c` にあるため、4-byte 境界のまま libfdt / bootm に渡さない。
- byte offset と NAND page index の変換は driver 境界で一度だけ行う。
- Image1 は DHP2、Image2 は raw FIT と expected format を固定する。
- rootfs 指定を Selector から注入せず、OEM の Linux parser と OpenWrt の UBI 構成を
  それぞれ使う。
- WB は read を行わず Console に止める。
- Selector の永続更新後も、非対象領域 hash は変化しなかった。

## 既知の制約

- 実機の UART 物理経路は安定性が低く、高速な送信には使えない。
- NAND の ECC / OOB 形式は driver 依存であり、別の Recovery や個体へ raw image を
  そのまま移植できるとは限らない。
- OEM firmware の内部データは配布していないため、再現者は適法に取得した自分の
  image を用意する必要がある。
- 本 Selector は今回の固定構成を対象とし、汎用的な slot 自動修復機能を持たない。
- Selector の NAND reader は物理 page を連続して読み、bad block を skip / remap
  しない。検証個体では全 1024 eraseblock の scan で read error 0、bad marker 0
  だった。別個体では各 boot 対象範囲の bad block 状態を事前確認し、1個でも
  存在する場合は現在の固定 offset 構成をそのまま使用しない。
- Ethernet は純正ブートチェーンから引き継いだ switch / SGMII 状態に依存し、
  custom U-Boot 単独の cold initialization は未実装である。
- FIT の crc32 / sha1 は破損検出であり、署名による真正性保証ではない。
