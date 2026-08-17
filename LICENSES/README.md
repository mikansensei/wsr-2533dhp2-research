# License map

| 範囲 | SPDX | ライセンス |
| --- | --- | --- |
| `README.md`, `NOTICE.md`, `PROVENANCE.md`, `docs/`, `data/`, `evidence/`, `examples/` | `CC-BY-4.0` | Creative Commons Attribution 4.0 International |
| `scripts/` | `MIT` | MIT License |
| `src/uboot/*.c`, `*.dts`, `*.env`, `configs/`, `patches/u-boot/` | `GPL-2.0-or-later` | GNU GPL v2.0 or later |
| `src/uboot/README.md`, `src/openwrt/README.md`, `src/recovery/README.md` | `CC-BY-4.0` | CC BY 4.0 |
| `src/openwrt/*.dtsi`, `src/recovery/*.dtsi` | `GPL-2.0-or-later OR MIT` | 上流DTSと互換のdual license |

個別ファイルにSPDX行がある場合はその表記を優先する。path単位の指定はリポジトリ
直下の[`REUSE.toml`](../REUSE.toml)にも記録する。

公式ライセンスページ:

- CC BY 4.0: <https://creativecommons.org/licenses/by/4.0/>
- MIT: <https://opensource.org/license/mit/>
- GPL-2.0: <https://www.gnu.org/licenses/old-licenses/gpl-2.0.html>

`CC-BY-4.0.txt`は公式legal codeへの参照とattribution noticeであり、ライセンス
条件の正式な内容はCreative Commonsのlegal codeを参照する。
