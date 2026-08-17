<!-- SPDX-License-Identifier: CC-BY-4.0 -->

# 機器と起動チェーンの分析

## 対象

| 項目 | 観測値 |
| --- | --- |
| 製品 | Buffalo WSR-2533DHP2 |
| SoC / ISA | MediaTek MT7622 / ARMv8-A |
| 純正 U-Boot execution state | AArch32 |
| custom Stage 2 / Linux | AArch64 |
| DRAM | 256 MiB、custom U-Boot DTでは`0x40000000 + 0x0f000000`を使用 |
| NAND | Winbond W29N01HZ(S)INF 相当の 128 MiB SLC |
| NAND ID | `ef a1 00 95 00` |
| page | 2,048 bytes |
| OOB | 64 bytes |
| eraseblock | 128 KiB / 64 pages |
| ECC | 4-bit ECC 経路を使用 |
| UART | 115200 bps、8N1 |
| WLAN | MediaTek MT7615 PCIe |
| Ethernet switch | RTL8367系、実機Linux logは`rtl8367s_swconfig_init`を表示 |

NAND の raw page、OOB、ECC の扱いは U-Boot の単純な byte offset 読出しと Linux
MTD の logical read で一致しないことがある。書込み検証ではこの差を前提にし、
可能な限り Linux NAND driver 経由の操作を優先した。

## 純正ブートチェーン

BootROM から Preloader、ATF、純正 U-Boot 2014.04-rc1 へ進む。純正 U-Boot は
メニューと dual-image 系の処理を持つが、異種 image を並存させる最終構成では
自動同期・修復処理を使わないことが安全だった。

最終構成ではこのバイナリチェーンを変更せず、純正 U-Boot が NAND 末尾の
Selector slotを起動する経路を利用する。そのため Config の `boot2` は一度だけ
永続変更したが、以後の Selector 更新では環境保存を行わない。

## GPIO とスイッチ

AUTO / MANUAL の二つのスイッチは今回の起動選択から外し、ROUTER / AP / WB の
二つの GPIO だけを判定した。

| 位置 | GPIO1 | GPIO16 | Selector の動作 |
| --- | ---: | ---: | --- |
| ROUTER | 0 | 1 | Image1 / `EXPECT_DHP2` |
| AP | 1 | 1 | Image2 / `EXPECT_RAW_FIT` |
| WB | 1 | 0 | Console 停止、NAND read なし |
| 未知 | 0 | 0 など | 安全停止 |

GPIO は起動直後に読み、既知の組み合わせ以外は起動しない。WB では、Console へ
停止すること自体が受入れ条件であり、不要な NAND read を行わない。

## UART とネットワーク

UART は観測に便利だが、使用した USB-UART アダプタは高速な連続送信で文字欠落・
文字化けを起こした。最終的な運用ルールは [安全な実験運用](08-operation-safety.md)
にまとめている。大容量 artifact の受け渡しは UART ではなく TCP/IP / TFTP を
使い、UART は短い制御とログ取得に限定した。

機器の制御用ネットワークと、実験環境の上位ネットワークは分離して扱う。公開
資料には個体固有のアドレス、MAC、認証情報、ホスト上のパスを記載しない。

custom U-Boot の Ethernet bring-up は、純正ブートチェーンから引き継いだ
switch / SGMII 状態に依存する。どの純正 stage が初期化を完了したかは確定して
おらず、switch の native cold initialization は実装していない。

## RAM上の主要address

| address | 用途 |
| ---: | --- |
| `0x41000000` | NAND header / FITのstage buffer、純正U-BootからSelector FITを読む場所 |
| `0x41800000` | 検証・比較用buffer |
| `0x40080000` | 未整列DHP2 inner FITをコピーするboot用整列address |
| `0x44000000` | custom U-BootまたはLinux kernelのload / entry |

FITを`0x40080000`へ置いた後、`bootm`がkernelを`0x44000000`へ展開する。対象FITは
6 MiB slot内に制限し、stage・boot先・kernel展開先が衝突しないことを確認した。
