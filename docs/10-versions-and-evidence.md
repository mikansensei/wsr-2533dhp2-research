<!-- SPDX-License-Identifier: CC-BY-4.0 -->

# 版、artifact、実験証拠

## Software matrix

| 対象 | 観測・固定した版 |
| --- | --- |
| 純正 U-Boot | `U-Boot 2014.04-rc1 (Apr 18 2018 - 19:44:31) 0.05` / AArch32 |
| custom U-Boot base | `527115ef6783cec49e5610c523c124b399011361` |
| custom U-Boot表示 | `U-Boot 2026.10-rc2-g527115ef6783-dirty` / AArch64 |
| U-Boot toolchain | GCC 13.3.0、binutils 2.42 |
| OpenWrt実機build | `r33051-f5dae5ece4`、Linux 6.12.94 |
| OEM Linux | 4.4.92 |

`dirty`は公開patchとして整理する前の研究treeでbuildしたことを示す。公開recipeは
同じbase commitへ差分を適用するが、日時・tree state・tool versionまで完全に
固定したbit-for-bit reproducible buildを主張しない。歴史上のartifact hashは
[data/artifact-hashes.tsv](../data/artifact-hashes.tsv)に記録する。

## Evidence index

[evidence/index.tsv](../evidence/index.tsv)は、公開上の主張とサニタイズ済み抜粋を
対応付ける。抜粋はprivate address、MAC、ホストpath、認証情報を除去した。元ログ
全体は未公開であり、抜粋中の`[redacted]`や`[excerpt]`は省略を表す。

最終受入れのうち、ROUTER自動起動とSelector書込みverifyは保存ログで確認した。
AP/WBを含む最終三位置試験はユーザーによる実機観測を受入れ結果として記録した。

## Observation class

- `captured`: UARTまたはRecoveryログに保存された直接観測
- `user-observed`: 実機操作者から報告された受入れ結果
- `derived`: offset、page、size等から算出した値
- `source-derived`: 公開上流sourceを参照して実装した値・処理
