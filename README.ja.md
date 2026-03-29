# Noctis Viewer

[English](README.md) | [简体中文](README.zh-CN.md) | [日本語](README.ja.md)

Noctis Viewer は、Windows 向けの軽量なネイティブ画像ビューアです。高速起動、素早い画像切り替え、そして AI 画像の生成情報確認に特化しています。

このソフトは主に次の 2 つの課題を解決するために作られています。

1. 従来の画像ビューアは機能が多すぎて重く、日常的によく使う用途に対して起動が遅いことが多い。
2. 従来の画像ビューアは AI 画像に埋め込まれた生成情報を表示できないことが多く、ローカル画像の prompt や生成パラメータをすばやく確認しにくい。

特に AI 画像ワークフロー向けに、PNG に WebUI 互換の生成情報が含まれている場合、Noctis Viewer はその情報を右側の 2 列テーブルで表示します。一方で、ComfyUI の workflow JSON は意図的に表示せず、見やすさを優先しています。

![Noctis Viewer preview](assets/preview.png)

## 特長

- ネイティブ Win32 + GDI+ アプリケーション
- 軽量で高速起動
- PNG、JPG、JPEG、BMP、GIF、TIF、TIFF をサポート
- 矢印キーで前の画像 / 次の画像へ移動
- `Page Up` / `Page Down` でズームイン / ズームアウト
- 空白領域をダブルクリックして画像を開く
- 読み込み時にウィンドウへ自動フィット
- ウィンドウサイズ変更時も自動で再フィット
- ダーク UI
- 右側メタデータパネル：
  - 2 列テーブル表示
  - 長い値は自動改行
  - ヘッダークリックで折りたたみ / 再展開
  - 値セルクリックでクリップボードへコピー
- `Delete` キーで現在の画像を確認付きで削除
- カスタムアプリアイコン付き

## メタデータ表示方針

Noctis Viewer は WebUI 互換の情報のみを表示します。

- 表示対象：`parameters`、プレーンテキストの prompt
- 表示しない：ComfyUI の `prompt` / `workflow` JSON

これにより、巨大な workflow データをそのまま表示せず、必要な生成情報だけを素早く確認できます。

## 推奨する ComfyUI での保存方法

ComfyUI では、次のプロジェクトの Prompt Saver Node を使って画像を保存することをおすすめします。

https://github.com/receyuki/comfyui-prompt-reader-node

この Prompt Saver Node で保存した画像は Noctis Viewer と非常に相性がよく、特に本プロジェクトのメタデータパネルと組み合わせると快適に使えます。

## 操作方法

- `Left`、`Up`：前の画像
- `Right`、`Down`：次の画像
- `Page Up`：ズームイン
- `Page Down`：ズームアウト
- `Delete`：現在の画像を削除（確認あり）
- マウスホイール：前 / 次の画像
- 空白領域をダブルクリック：画像を開く
- メタデータ値セルをクリック：値をコピー

## プロジェクト構成

```text
NoctisViewer/
├── assets/
│   ├── noctis_viewer.ico
│   └── noctis_viewer_icon.png
├── CMakeLists.txt
├── noctis_viewer.cpp
├── noctis_viewer.rc
├── resource.h
├── build_native.bat
└── README.md
```

## ビルド

### 必要環境

- Windows
- CMake 3.10+
- Visual Studio Build Tools または C++ ワークロードを含む Visual Studio

### CMake でビルド

```powershell
cmake -S . -B build
cmake --build build --config Release
```

出力ファイル：

```text
bin\Release\Noctis_Viewer.exe
```

### 補助スクリプトでビルド

```powershell
.\build_native.bat
```

## 今後の候補

- ごみ箱へ移動する削除モード
- サムネイルバー / フォルダサイドバー
- ページ切り替え時の先読み
- 配布用ポータブルパッケージの改善

## License

このプロジェクトは MIT License を採用しています。詳細は [LICENSE](LICENSE) を参照してください。
