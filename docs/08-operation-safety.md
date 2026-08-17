<!-- SPDX-License-Identifier: CC-BY-4.0 -->

# 安全な実験運用

## UART 送信

使用した USB-UART アダプタは高速送信に耐えなかった。以後の標準ルールは次の
通りである。

1. 送信は常に 1 byte ずつ行い、標準間隔は 120 ms/byte 以上とする。
2. 長いコマンド、複数コマンド、script、binary を UART へ一括投入しない。
3. 改行は全 byte の送信後に単独で送り、prompt と応答を確認してから次へ進む。
4. `screen -X stuff` などの一括投入を使わない。
5. TFTP、Recovery image、Flash artifact は TCP/IP で転送する。
6. 欠落、文字化け、未完了 prompt、想定外応答が出たら入力を停止し、ログを保存
   して再送しない。

## ネットワーク境界

機器の管理は専用の管理 LAN から行い、上位ネットワークのルータを機器制御に
使用しない。大容量ファイルの転送が必要な場合も、TFTP の対象ファイル配置と
hash 確認に限定し、上位ルータの reboot や一般設定変更を行わない。

公開資料では、実験時の private address、MAC、認証情報、TFTP の個体依存ファイル
名を記録しない。

## `/tmp` の artifact 管理

Recovery は RAM と `/tmp` の容量が限られる。大容量 artifact を転送する場合は、
一度に必要な一つだけを配置する。

```text
転送 -> size 確認 -> SHA-256 確認 -> 使用 -> readback / verify
                                      -> 不要 artifact を削除
```

再起動をまたぐ `/tmp` の存在を前提にしない。古い image、duplicate、失敗した
部分転送を残さず、次の作業前に使用量と不要ファイルを確認する。

## 永続領域の保護

Preloader、ATF、純正 U-Boot、factory、glbcfg、board_data、非対象 image には
触れない。Config は最終 `boot2` の永続化後に保護対象とした。Selector には
write / erase / repair / fallback を実装しない。以後の環境変数変更は RAM 上の
一時的な検証に限定し、`saveenv` を実行しない。
