<!-- SPDX-License-Identifier: CC-BY-4.0 -->

# OEM Image1復元経路

## 今回最終的に使った経路

Image1の最終OEM復元には、Bacalhau氏のQiita記事
[OpenWrt化したWSR-2533DHP2をBuffalo純正ファームウェアに戻す](https://qiita.com/Bacalhau/items/3ee93499083bcaffcd91)
の「簡単編」と同じ経路を使用した。公式配布物は
[Buffaloの製品サポートページ](https://www.buffalo.jp/product/detail/software/wsr-2533dhp2-cb.html)
から利用者自身が取得する。

検証時の対象はversion 1.28だった。配布ZIPから暗号化済みfirmwareを取り出し、
記事で案内されている`firmware-wintools` 0.8.0で次の変換を行った。

```text
firmware-wintools buffalo-enc \
  -i wsr_2533dhp2_jp_128 \
  -o wsr_2533dhp2_jp_128.dec \
  -d -O 0xc8 -l
```

観測されたmetadataはproduct `WSR-2533DHP2`、version `1.28`、data length
`11608064` bytesだった。生成された`.dec`をOpenWrtの`firmware` partitionへ
Linux MTD経由で書く方法でOEMへ復元した。このリポジトリは入力・復号後artifact・
復号toolのbinaryを配布しない。

記事の簡単編が示す書込みcommandは次であり、`-r`は完了後のrebootを伴う。

```text
mtd -r write wsr_2533dhp2_jp_128.dec firmware
```

今回これは実機操作者がOEM復元を行った独立工程として使用した。後続のSelector
更新Recoveryに適用した「自動reboot禁止」とは工程が異なる。

今回使用した復号後artifactのSHA-256は
`0e2dd15f26f56a4fcc61d0e82e394f109d6aff7cf8cabb903a180327d2476c71`である。この値は
version 1.28の今回の入力を識別する記録であり、将来版に対する期待値ではない。

## 58 MiB backup復元との違い

研究途中には、Image1全体`0x00200000-0x03c00000`を58 MiBのlogical backupから
専用Recoveryで復元する経路も検証した。これは公式配布版から作る約11.1 MiBの
recovery dataとは別物である。

最終OEM状態の確認は、公式配布版由来の`.dec`を書いた後に行った。従って公開資料
では次を混同しない。

- 58 MiB logical backup: slot全体のbyte-for-byte復元用
- 公式配布版由来`.dec`: OpenWrtのBuffalo/TRX対応`mtd write`へ渡す入力

## 適用前提と安全条件

Qiita記事の一行コマンドは標準的なOpenWrt partition layoutを前提にする。本研究の
custom layoutへそのまま適用してよいという意味ではない。実行前に`firmware`の
offsetが`0x00200000`、sizeが`0x03a00000`であり、Image2 `0x03c00000`以降を含まない
ことを`/proc/mtd`とsysfsで確認する必要がある。

最終Hybrid構成ではImage2とSelectorを保護する必要があるため、partition labelだけ
を信用した再実行、自動rebootを伴うblind write、`dual_image_check`との併用を
行わない。変換toolや公式firmwareのversionが変わった場合も、size、DHP2 magic、
`+0x1c`のFIT、hashを改めて検査する。

記事で使うWSR自身の`/tmp`はRAM上の転送先である。上位ネットワークのコアルータの
`/tmp`とは別であり、本研究では後者へartifactを蓄積しない。WSR上でも転送物は
size/hash確認後に使用し、不要になった旧imageを順次削除する。
