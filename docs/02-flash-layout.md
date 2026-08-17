<!-- SPDX-License-Identifier: CC-BY-4.0 -->

# Flash layout と保護境界

## 最終的に扱った論理 layout

| offset | size | label / 内容 | 公開資料での扱い |
| ---: | ---: | --- | --- |
| `0x000000` | 512 KiB | Preloader | 保持、書込み禁止 |
| `0x080000` | 256 KiB | ATF | 保持、書込み禁止 |
| `0x0c0000` | 512 KiB | 純正 U-Boot | 保持、書込み禁止 |
| `0x140000` | 512 KiB | Config / env | `boot2`のみ変更済み、以後保護 |
| `0x1c0000` | 256 KiB | factory | 保持、書込み禁止 |
| `0x200000` | 58 MiB | Image1 OEM slot | OEM 構造を保持 |
| `0x3c00000` | 6 MiB | Image2 FIT | OpenWrt FIT |
| `0x4200000` | 52 MiB | Image2 UBI | OpenWrt rootfs / overlay |
| `0x7600000` | 2 MiB | glbcfg | 保持、書込み禁止 |
| `0x7800000` | 2 MiB | board_data | 保持、書込み禁止 |
| `0x7a00000` | 6 MiB | Boot Selector | Selector v3 のみ |

128 MiB NAND の上限は `0x8000000` である。Image1 は OEM firmware 全体を保持
する論理 slot として扱う一方、Recovery DTB では検証上 `image1-fit` と
`image1-ubi` に分けて見せる場合があった。この表示上の分割と OEM DHP2/TRX の
内部構造を混同しない。

## byte offset と page index

Selector の公開 API は byte offset に統一した。しかし既存の NAND reader が page
index を要求するため、driver 境界で一度だけ次の変換を行う。

```text
page_index = byte_offset / writesize
writesize  = 2048

Image1: 0x00200000 / 0x800 = 0x0400
Image2: 0x03c00000 / 0x800 = 0x7800
```

ログには byte offset と page index の両方を出す。上位で page に変換した値を下位
API が再び変換しないことが重要である。過去に Image2 の offset と page を混同
した読出しが発生したため、現在の Selector では境界を明示している。

## 書込み境界

Hybrid Selector v3 の最終永続化工程で更新したのは Selector slot のみである。
最終構成の準備では、別工程として Image1 の OEM 復元、Image2 の OpenWrt 復元、
Config の `boot2` 更新を実施した。Image2 の FIT / UBI 復元は専用 Recovery
で対象を固定し、Image1 と保護領域を read-only として検査した。書込み前後に
非対象 partition の hash を比較し、Selector artifact の readback と verify を
別に行った。

Selector は自身の Flash を書き換えない。異常時の repair、fallback、Image1/Image2
相互コピー、dual-image 同期も実装していない。
