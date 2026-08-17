<!-- SPDX-License-Identifier: CC-BY-4.0 -->

# 調査・実装タイムライン

## 1. 事前確認

- UART console を確立し、純正 U-Boot と Linux の両方を識別した。
- UART の高速送信で欠落が起きることを確認し、以後は一 byte ずつの低速送信を
  必須とした。
- 大容量転送は TCP/IP / TFTP に移し、UART は監視と短い制御に限定した。
- NAND の page、OOB、eraseblock、ECC と論理 partition を整理した。

## 2. 純正 OEM の調査

- 純正 U-Boot の version / environment / Image1・Image2 の boot path を取得した。
- Image1 の DHP2 wrapper と inner FIT の `+0x1c` 関係を確認した。
- OEM Linux の TRX parser が firmware 内部を分割し、rootfs を認識することを確認した。
- FIT 直指定で kernel / FDT まで起動しても rootfs が見えないケースを再現し、
  kernel/FDT と rootfs 構造の互換性を分けて考える必要を得た。

## 3. NAND read と Recovery

- U-Boot 側で raw NAND read の page / byte 境界を切り分けた。
- Linux Recovery で MTD geometry と read-only 境界を確認した。
- Image2 UBI を Linux UBI toolchain で再構築し、raw hash ではなく logical volume
  と geometry で検証する方針にした。
- `/tmp` に古い大容量 artifact を残さず、転送後の hash と不要ファイルの整理を
  必須ルールにした。

## 4. Selector の RAM 検証

- GPIO から ROUTER / AP / WB を分ける read-only prototype を作った。
- Image2 raw FIT の起動を確認した。
- Image1 DHP2 の unaligned inner FIT が alignment error になることを確認した。
- FIT body を整列済み RAM へコピーする v3 を作り、OEM Linux 4.4.92 と SquashFS
  rootfs の起動を確認した。

## 5. Selector の永続化と受入れ

- 純正`boot2`を末尾Selector FITのread / verify / bootへ一度だけ永続変更した。
- Image1を公式配布版1.28から作成したrecovery dataでOEMへ復元した。
- Image2を6 MiB FIT + 52 MiB UBIのOpenWrtへ復元した。
- Selector だけを writable とした専用 Recovery で 6 MiB slot を更新した。
- `mtd verify` と readback SHA-256 が一致した。
- 書込み後の ROUTER 条件で OEM 自動起動を確認した。
- 最後にユーザーが三つの物理スイッチ位置を確認し、ROUTER=OEM、AP=OpenWrt、
  WB=Console を受け入れた。

## 6. 主要な失敗から得た知見

- U-Boot menuのUART自動操作は送信欠落を起こしたため、常時低速送信へ変更した。
- DHP2 `+0x1c`の未整列FITを直接libfdt/bootmへ渡す方式を廃止した。
- U-Boot raw NAND writeとLinux ECC/OOB表現を混在させず、永続書込みはLinux MTDを
  優先した。
- UBIはraw 52 MiB SHA一致ではなく、volume geometryとlogical contentで検証した。
- 上位ルータの`/tmp`へartifactを蓄積して障害を起こしたため、一時ファイルの
  一個運用・即時削除・上位ルータ非操作をルール化した。
