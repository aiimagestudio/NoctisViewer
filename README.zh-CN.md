# Noctis Viewer v1.4.1

[English](README.md) | [简体中文](README.zh-CN.md) | [日本語](README.ja.md)

Noctis Viewer 是一个面向 Windows 的轻量级原生图片浏览器，专注于快速打开、快速翻阅和快速查看 AI 图片生成信息。

当 PNG 中包含 WebUI 兼容的生成信息时，Noctis Viewer 会在右侧以两列表格形式展示这些信息，并且刻意忽略 ComfyUI 的 workflow JSON，让界面保持清爽、可读。

从 v1.3 开始，Noctis Viewer 还内置了更强大的 HaldCLUT 工作流，支持实时浏览、预览、对比和导出 LUT 效果，因此它不仅适合查看提示词与生成参数，也非常适合快速检查图片调色结果。

![Noctis Viewer preview](assets/preview.png)
![Noctis Viewer LUT preview](assets/preview_lut.png)

## 特性

- 原生 Win32 + GDI+ 应用
- 体积小，启动快
- 完整的高 DPI 显示支持（100% - 300% 缩放）
- 支持 PNG、JPG、JPEG、BMP、GIF、TIF、TIFF
- 方向键切换上一张 / 下一张
- `Page Up` / `Page Down` 放大 / 缩小
- `Ctrl + 鼠标滚轮` 平滑缩放
- `Home` / `End` 跳转到第一张 / 最后一张
- `Ctrl+O` 打开文件对话框
- 双击空白区打开图片
- 加载时自动适应窗口，调整窗口大小时自动重新适应
- 状态栏实时显示缩放比例
- 放大时平滑的鼠标拖拽平移
- 右侧元数据面板支持点击复制
- HaldCLUT 面板支持递归扫描 LUT 目录
- HaldCLUT 面板顶部提供 `Original`，可一键回到原图
- 在 LUT 生效时按住 `Space` 可临时预览原图
- HaldCLUT 提供两种应用模式：
  - `MX_LUT Compatible`
  - `Smooth Interpolation`
- `文件 > Save Current Preview As...` 可导出当前预览结果
- 支持文件关联（添加到"打开方式"菜单）
- 菜单栏支持（文件、视图、工具、帮助）
- 双缓冲优化渲染，消除闪烁

## v1.4.0 更新内容

- **高 DPI 显示支持**：每显示器 DPI 感知，在所有显示器上清晰渲染
- **Ctrl + 鼠标滚轮**：按住 Ctrl 键平滑缩放
- **Home / End 键**：快速跳转到第一张和最后一张图片
- **图像质量改进**：使用高质量 GDI+ 渲染设置，缩放更清晰
- **消除面板闪烁**：双缓冲实现平滑滚动和拖拽
- **缓存背景缓冲**：硬件加速的平滑平移（放大时）
- **异步 LUT 加载**：选择 LUT 时 UI 响应更快（选中状态立即更新）

## v1.3.1 更新内容

- 小键盘缩放支持（`小键盘 +` / `小键盘 -`）
- 放大时支持鼠标拖拽平移
- 修复拖拽图片时的面板闪烁问题
- 修复 HaldCLUT 加载进度对话框

## 元数据显示规则

Noctis Viewer 只显示 WebUI 兼容信息。

- 支持：`parameters`、纯文本 prompt
- 忽略：ComfyUI 的 `prompt` / `workflow` JSON

## HaldCLUT 致谢

开发和测试过程中使用的 HaldCLUT 资源来自：

https://github.com/cedeber/hald-clut

感谢 `hald-clut` 项目及其贡献者提供并维护这套非常优秀的 LUT 资源。

## 快捷键

| 按键 | 功能 |
|------|------|
| `Left`、`Up` | 上一张 |
| `Right`、`Down` | 下一张 |
| `Page Up` / `小键盘 +` | 放大 |
| `Page Down` / `小键盘 -` | 缩小 |
| `Ctrl + 鼠标滚轮` | 放大 / 缩小 |
| `Home` | 跳转到第一张图片 |
| `End` | 跳转到最后一张图片 |
| `Ctrl+O` | 打开文件对话框 |
| `Delete` | 删除当前图片（需确认） |
| `H` | 切换 HaldCLUT 面板 |
| `Space` | 在 LUT 生效时按住临时预览原图 |
| 鼠标滚轮 | 上一张 / 下一张 |
| 拖拽（放大时） | 平移图片 |
| 双击空白区 | 打开图片 |
| 点击元数据值 | 复制值到剪贴板 |
| 点击 HaldCLUT 条目 | 应用 LUT |
| 点击 `Original` | 恢复原图显示 |

## 构建

### 环境要求

- Windows 10 或更高版本
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

## 下载

- `Noctis_Viewer-v1.4.1-x64.zip`

解压后直接运行 `Noctis_Viewer.exe`，无需安装。

## 系统要求

- Windows 10 或更高版本
- 高 DPI 显示支持（已测试 100% - 300% 缩放）
- 无需额外依赖

## GitHub

<https://github.com/aiimagestudio/NoctisViewer>

## License

本项目采用 MIT License。详见 [LICENSE](LICENSE)。
