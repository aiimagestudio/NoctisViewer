# Noctis Viewer v1.3.0

[English](README.md) | [简体中文](README.zh-CN.md) | [日本語](README.ja.md)

Noctis Viewer is a fast, minimal native image viewer for Windows, designed for quickly browsing PNG, JPG, and other common image formats with zero runtime dependencies.

It is especially useful for AI image workflows: when a PNG contains WebUI-compatible generation metadata, Noctis Viewer shows it in a clean two-column side panel while intentionally ignoring ComfyUI workflow JSON.

Starting with v1.3, Noctis Viewer also includes a powerful built-in HaldCLUT workflow for browsing, previewing, comparing, and exporting LUT-based looks in real time, making it a practical viewer for both prompt inspection and quick color grading review.

![Noctis Viewer preview](assets/preview.png)
![Noctis Viewer LUT preview](assets/preview_lut.png)

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
- Metadata side panel with click-to-copy values
- HaldCLUT panel with recursive directory scan
- `Original` entry to reset back to the unfiltered image
- Hold `Space` to preview the original image while a LUT is active
- HaldCLUT apply modes:
  - `MX_LUT Compatible`
  - `Smooth Interpolation`
- `File > Save Current Preview As...` to export the current preview
- File association support (add to "Open with" menu)
- Menu bar with File, View, Tools, and Help options

## New in v1.3.0

- Added HaldCLUT mode switching in the View menu
- Added `Original` item at the top of the HaldCLUT panel
- Added `Save Current Preview As...` for exporting the current LUT preview
- Fixed HaldCLUT application to match ComfyUI `MX_LUT` behavior
- Added an optional smooth interpolated LUT mode
- Fixed `Space` key conflict with the Generation Info header
- Fixed HaldCLUT selection and reset behavior
- Fixed HaldCLUT panel `[Configure]` click handling

## Metadata Behavior

Noctis Viewer only shows WebUI-compatible metadata fields.

- Supported: `parameters`, plain prompt text
- Ignored on purpose: ComfyUI `prompt` / `workflow` JSON graphs

## HaldCLUT Credits

The HaldCLUT collection used during development and testing was sourced from:

https://github.com/cedeber/hald-clut

Many thanks to the `hald-clut` project and its contributors for providing and maintaining this excellent LUT collection.

## Controls

- `Left`, `Up`: previous image
- `Right`, `Down`: next image
- `Page Up`: zoom in
- `Page Down`: zoom out
- `Ctrl+O`: open file dialog
- `Delete`: delete current image after confirmation
- `H`: toggle HaldCLUT panel
- `Space` (hold): preview original image while LUT is active
- Mouse wheel: previous / next image
- Double-click empty area: open image
- Click metadata value cell: copy value to clipboard
- Click a HaldCLUT entry: apply LUT
- Click `Original` in the HaldCLUT panel: clear LUT selection

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

- `Noctis_Viewer-v1.3.0-x64.zip`

Extract and run `Noctis_Viewer.exe`. No installation required.

## GitHub

<https://github.com/aiimagestudio/NoctisViewer>

## License

This project is licensed under the MIT License. See [LICENSE](LICENSE).
