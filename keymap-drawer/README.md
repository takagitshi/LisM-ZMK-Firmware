# keymap-drawer

キーマップを SVG で可視化するための設定です。

## 手動生成（GitHub Actions）

通常のpushでは、実機用firmwareを生成するworkflowだけが自動実行されます。

SVGを更新するときは、GitHubのActions画面で`Draw ZMK keymaps`を選び、`Run workflow`から手動実行してください。生成結果に差分がある場合は、`[Draw] Update rendered keymaps`というcommitで反映されます。

---

## ローカルでの実行（Mac）

ローカルで SVG を確認したい場合の手順です。  
コマンドはすべて **`zmk-config-LisM` フォルダ直下で実行**してください。

### インストール

#### pipx のインストール

```bash
brew install pipx
```

#### keymap-drawer のインストール

```bash
pipx install keymap-drawer
pipx ensurepath
source ~/.zshrc
```

### ファイル構成

```
keymap-drawer/
├── config.yaml   # keymap-drawer の設定（ラベル変換・描画設定）
└── lism.yaml     # parse済みのキーマップ（自動生成）

config/
├── lism.keymap   # ZMK キーマップファイル
└── lism.json     # キーボードの物理レイアウト定義
```

### コマンド

#### parse（.keymap → .yaml）

```bash
keymap -c keymap-drawer/config.yaml parse -z config/lism.keymap -o keymap-drawer/lism.yaml
```

#### draw（.yaml → .svg）

```bash
keymap -c keymap-drawer/config.yaml draw keymap-drawer/lism.yaml -j config/lism.json -o keymap-drawer/lism.svg
```

#### まとめて実行

```bash
keymap -c keymap-drawer/config.yaml parse -z config/lism.keymap -o keymap-drawer/lism.yaml && \
keymap -c keymap-drawer/config.yaml draw keymap-drawer/lism.yaml -j config/lism.json -o keymap-drawer/lism.svg
```
