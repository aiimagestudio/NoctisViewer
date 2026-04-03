# Noctis Viewer v1.3.0

[English](README.md) | [简体中文](README.zh-CN.md) | [日本語](README.ja.md)

Noctis Viewer は、Windows 向けの軽量なネイティブ画像ビューアです。高速起動、素早い画像切り替え、そして AI 画像の生成情報確認に特化しています。

PNG に WebUI 互換の生成情報が含まれている場合、Noctis Viewer はその情報を右側の 2 列テーブルで表示します。一方で、ComfyUI の workflow JSON は意図的に表示せず、見やすさを優先しています。

v1.3 からは強力な HaldCLUT ワークフローも内蔵され、LUT の参照、リアルタイムプレビュー、比較、書き出しまで行えるようになりました。生成情報の確認だけでなく、手早いカラーグレーディング確認ツールとしても活用できます。

![Noctis Viewer preview](assets/preview.png)
![Noctis Viewer LUT preview](assets/preview_lut.png)

## 特長

- ネイティブ Win32 + GDI+ アプリケーション
- 軽量で高速起動
- PNG、JPG、JPEG、BMP、GIF、TIF、TIFF をサポート
- 矢印キーで前の画像 / 次の画像へ移動
- `Page Up` / `Page Down` でズームイン / ズームアウト
- `Ctrl+O` でファイルダイアログを開く
- 空白領域をダブルクリックして画像を開く
- 読み込み時にウィンドウへ自動フィット
- ステータスバーにズーム率を表示
- メタデータパネルで値をクリックしてコピー可能
- HaldCLUT パネルで LUT を再帰的に読み込み
- HaldCLUT パネル先頭に `Original` を表示
- LUT 適用中に `Space` を押し続けると元画像を一時プレビュー
- HaldCLUT 適用モード:
  - `MX_LUT Compatible`
  - `Smooth Interpolation`
- `File > Save Current Preview As...` で現在のプレビューを書き出し
- ファイル関連付けサポート
- メニューバー（File / View / Tools / Help）

## v1.3.0 の変更点

- View メニューに HaldCLUT モード切替を追加
- HaldCLUT パネル先頭に `Original` 項目を追加
- `Save Current Preview As...` を追加
- HaldCLUT 適用ロジックを ComfyUI `MX_LUT` に合わせて修正
- オプションのスムーズ補間モードを追加
- `Space` キーと Generation Info ヘッダーの競合を修正
- HaldCLUT の選択とリセット挙動を修正
- HaldCLUT パネルの `[Configure]` クリック処理を修正

## メタデータ表示方針

Noctis Viewer は WebUI 互換の情報のみを表示します。

- 表示対象: `parameters`、プレーンテキストの prompt
- 表示しない: ComfyUI の `prompt` / `workflow` JSON

## HaldCLUT クレジット

開発およびテストで使用した HaldCLUT コレクションは、次のプロジェクトから取得しました。

https://github.com/cedeber/hald-clut

すばらしい LUT コレクションを公開・保守している `hald-clut` プロジェクトと貢献者の皆さまに感謝します。

## 操作方法

- `Left`、`Up`: 前の画像
- `Right`、`Down`: 次の画像
- `Page Up`: ズームイン
- `Page Down`: ズームアウト
- `Ctrl+O`: ファイルダイアログを開く
- `Delete`: 現在の画像を削除（確認あり）
- `H`: HaldCLUT パネルの表示切替
- `Space`: LUT 適用中に押し続けて元画像をプレビュー
- マウスホイール: 前 / 次の画像
- 空白領域をダブルクリック: 画像を開く
- メタデータ値セルをクリック: 値をコピー
- HaldCLUT 項目をクリック: LUT を適用
- `Original` をクリック: 元画像表示に戻す

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

出力:

```text
bin\Release\Noctis_Viewer.exe
```

### 補助スクリプトでビルド

```powershell
.\build_native.bat
```

## ダウンロード

- `Noctis_Viewer-v1.3.0-x64.zip`

解凍して `Noctis_Viewer.exe` を実行するだけです。インストール不要です。

## GitHub

<https://github.com/aiimagestudio/NoctisViewer>

## License

このプロジェクトは MIT License を採用しています。詳細は [LICENSE](LICENSE) を参照してください。
