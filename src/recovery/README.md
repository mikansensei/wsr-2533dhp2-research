<!-- SPDX-License-Identifier: CC-BY-4.0 -->

# Linux RAM Recovery safety design

Recovery binaryや自動writerは公開しない。公開するのは、実際に用いたpartition
interlockの仕様である。全RecoveryでPreloader、ATF、Bootloader、Config、factory、
glbcfg、board_dataをread-onlyにし、目的外のImage slotもread-onlyにした。

| profile | writable range | label |
| --- | --- | --- |
| Image2 restore | `0x03c00000-0x07600000` | `image2-fit`, `image2-ubi` |
| Selector update | `0x07a00000-0x08000000` | `selector` |
| OEM Image1 full restore | `0x00200000-0x03c00000` | `oem-image1-restore` |

Recoveryは`rdinit=/init`でRAM上に起動し、書込み前にlabel、offset、size、erase size、
write size、sysfsのread-only属性、artifact hashを検査した。書込み後はreadbackを
行い、自動reboot、`saveenv`、`dual_image_check`を実行しなかった。

実装時は次の一profileだけを基のOpenWrt DTSの`nand@0/partitions`と置き換える。
複数profileを同時にwritableにしない。

- [image2-only-partitions.dtsi](image2-only-partitions.dtsi)
- [selector-only-partitions.dtsi](selector-only-partitions.dtsi)
- [oem-image1-only-partitions.dtsi](oem-image1-only-partitions.dtsi)

[partition-profiles.dtsi](partition-profiles.dtsi)は三profileの境界を短く比較する索引で
ある。各fragmentはpartition nodeだけなので、基DTSのNAND controller node内へ
統合し、`chosen`へ記載の`rdinit=/init`を設定する。
