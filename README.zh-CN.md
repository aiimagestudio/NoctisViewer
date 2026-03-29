# Noctis Viewer

[English](README.md) | [简体中文](README.zh-CN.md) | [日本語](README.ja.md)

Noctis Viewer 是一个面向 Windows 的轻量级原生图片浏览器，专注于快速打开、快速翻阅和快速查看 AI 图片生成信息。

它主要解决两个常见痛点：

1. 传统图片浏览器往往过于笨重，功能堆得很多，但日常高频真正会用到的功能并不多，同时启动速度也偏慢。
2. 传统图片浏览器通常无法显示 AI 图片中保存的生成信息，导致本地图片的 prompt 和参数不方便快速查阅。

对于 AI 图片工作流尤其如此：当 PNG 中包含 WebUI 兼容的生成信息时，Noctis Viewer 会在右侧以两列表格形式展示这些信息，并且刻意忽略 ComfyUI 的 workflow JSON，让界面保持清爽、可读。

![Noctis Viewer preview](assets/preview.png)

## 特性

- 原生 Win32 + GDI+ 应用
- 体积小，启动快
- 支持 PNG、JPG、JPEG、BMP、GIF、TIF、TIFF
- 方向键切换上一张 / 下一张
- `Page Up` / `Page Down` 放大 / 缩小
- 双击空白区打开图片
- 加载时自动适应窗口，调整窗口大小时自动重新适应
- 深色界面
- 右侧元数据面板支持：
  - 两列表格显示
  - 值内容自动换行
  - 点击表头折叠 / 展开
  - 点击值单元格复制内容
- 支持 `Delete` 键删除当前图片，并带确认弹窗
- 自带应用图标

## 元数据显示规则

Noctis Viewer 只显示 WebUI 兼容信息。

- 支持：`parameters`、纯文本 prompt
- 忽略：ComfyUI 的 `prompt` / `workflow` JSON

这样可以避免在侧栏里堆满庞杂的工作流数据，让本地图片参数查阅更直接。

## 推荐的 ComfyUI 保存方式

建议在 ComfyUI 中使用下面这个项目里的 Prompt Saver Node 来保存图片：

https://github.com/receyuki/comfyui-prompt-reader-node

通过它的 Prompt Saver Node 保存出来的图片，与 Noctis Viewer 的兼容性很好，尤其适合配合本项目的元数据面板一起使用。

## 快捷键

- `Left`、`Up`：上一张
- `Right`、`Down`：下一张
- `Page Up`：放大
- `Page Down`：缩小
- `Delete`：删除当前图片（需确认）
- 鼠标滚轮：上一张 / 下一张
- 双击空白区：打开图片
- 点击元数据值单元格：复制值到剪贴板

## 项目结构

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

## 构建

### 环境要求

- Windows
- CMake 3.10+
- Visual Studio Build Tools 或安装了 C++ 工作负载的 Visual Studio

### 使用 CMake 构建

```powershell
cmake -S . -B build
cmake --build build --config Release
```

输出文件：

```text
bin\Release\Noctis_Viewer.exe
```

### 使用辅助脚本构建

```powershell
.\build_native.bat
```

## 未来可扩展方向

- 回收站删除模式
- 缩略图栏 / 文件夹侧边栏
- 翻页预加载
- 更方便的便携版发布

## License

本项目当前使用 MIT License。详见 [LICENSE](LICENSE)。
