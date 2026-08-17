# Notice

## このアーカイブに含まれるもの

このリポジトリには、WSR-2533DHP2 を研究対象として得た次の資料を収録しています。

- ハードウェア、NAND geometry、Flash layout、GPIO、起動チェーンの観測記録
- U-Boot / FIT / DHP2 wrapper / UBI の構造に関する解析結果
- Hybrid Selector の設計、検証条件、実験結果
- 研究過程で作成したU-Bootの読み取り専用断片、統合patch、DTS、build recipe
- OpenWrt / Recoveryのpartition安全設計fragment
- 公開用に抽象化した表、擬似コード、再現チェックリスト、サニタイズ済みログ抜粋

## 含めていないもの

次のものは公開アーカイブへ収録していません。

- Buffalo の OEM firmware、復旧 firmware、factory image
- NAND の raw dump、bootloader dump、個体固有の calibration / board data
- OpenWrt、U-Boot、Arm Trusted Firmware の完全なソースツリーやビルド済み image
- UART の未編集ログ、ホスト固有の絶対パス、個体識別情報、認証情報、秘密鍵
- ローカルの TFTP 配置やネットワーク構成を再現するための個体依存設定

製品名、ベンダー名、Linux / OpenWrt / U-Boot などの名称は対象の識別と出典の
説明のために使用しています。製品名の使用は、製品提供者による承認や提携を
示すものではありません。

## 上流プロジェクト

- U-Boot: GPL-2.0-or-later。上流: <https://source.denx.de/u-boot/u-boot>
- Linux: GPL-2.0 系を中心とする複数ライセンス。上流: <https://www.kernel.org/>
- OpenWrt: GPL-2.0 系を中心とする複数ライセンス。上流: <https://openwrt.org/>
- Arm Trusted Firmware: 採用した版のライセンスと著作権表示を、その版の配布物で
  確認してください。本アーカイブには ATF のソースや binary は含めていません。

上流コードのライセンスを持つ部分を含む場合は、対象ファイルの SPDX 表記と
`LICENSES/` の対応する文書を優先します。OEM firmware の内容を転載するのでは
なく、観測した構造と挙動を記述しています。
