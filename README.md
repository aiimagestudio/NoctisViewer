# Noctis Viewer v1.1.0

[English](README.md) | [简体中文](README.zh-CN.md) | [日本語](README.ja.md)

Noctis Viewer is a fast, minimal native image viewer for Windows, designed for quickly browsing PNG, JPG, and other common image formats with zero runtime dependencies.

It is especially useful for AI image workflows: when a PNG contains WebUI-compatible generation metadata, Noctis Viewer shows it in a clean two-column side panel while intentionally ignoring ComfyUI workflow JSON.

Noctis Viewer is built to solve two common pain points:

1. Traditional image viewers are often too heavy, overloaded with features, and slower to launch than they need to be for everyday browsing.
2. Traditional image viewers usually cannot display AI image generation metadata, which makes it inconvenient to quickly inspect prompts and parameters stored in local images.

![Noctis Viewer preview](assets/preview.png)

## Highlights

- Native Win32 + GDI+ application
- Lightweight and fast startup
- Browse PNG, JPG, JPEG, BMP, GIF, TIF, and TIFF files
- Arrow keys for previous / next image
- `Page Up` / `Page Down` for zoom in / out
- `Ctrl+O` to open file dialog
- Double-click the empty area to open an image
- Auto-fit to window on load and resize
- Real-time zoom level display in status bar
- Dark UI optimized for quick viewing
- Metadata side panel with:
  - 2-column table layout
  - automatic text wrapping
  - collapsible header
  - click-to-copy value cells
- `Delete` key support with confirmation dialog
- File association support (add to "Open with" menu)
- Menu bar with File, Tools, and Help options
- Custom application icon included

## New in v1.1.0

- Added menu bar (File, Tools, Help)
- Added zoom level display in status bar
- Added file association feature (Tools → Set as Default Image Viewer)
- Added "Visit GitHub" menu item
- Added "About" dialog with version info
- Fixed various UI improvements

## Metadata Behavior

Noctis Viewer only shows WebUI-compatible metadata fields.

- Supported: `parameters`, plain prompt text
- Ignored on purpose: ComfyUI `prompt` / `workflow` JSON graphs

This keeps the side panel focused and readable instead of dumping large workflow payloads.

## Recommended ComfyUI Workflow

For the best compatibility, it is recommended to save images in ComfyUI with the Prompt Saver Node from this project:

https://github.com/receyuki/comfyui-prompt-reader-node

Images saved with its Prompt Saver Node are a great match for Noctis Viewer and work especially well with the metadata panel in this project.

## Controls

- `Left`, `Up`: previous image
- `Right`, `Down`: next image
- `Page Up`: zoom in
- `Page Down`: zoom out
- `Ctrl+O`: open file dialog
- `Delete`: delete current image after confirmation
- Mouse wheel: previous / next image
- Double-click empty area: open image
- Click metadata value cell: copy value to clipboard

## Project Structure

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

## Build

### Requirements

- Windows
- CMake 3.10+
- Visual Studio Build Tools or Visual Studio with C++ workload

### Build with CMake

```powershell
cmake -S . -B build
cmake --build build --config Release
```

Output:

```text
bin\Release\Noctis_Viewer.exe
```

### Build with helper script

```powershell
.\build_native.bat
```

## Download

Download the latest release:

- `Noctis_Viewer-v1.1.0-x64.zip`

Extract and run `Noctis_Viewer.exe`. No installation required.

## GitHub

https://github.com/aiimagestudio/NoctisViewer

## Roadmap Ideas

- Recycle Bin delete mode
- Thumbnail strip or folder sidebar
- Better image preloading for faster paging
- Portable release packaging

## License

This project is licensed under the MIT License. See [LICENSE](LICENSE).
