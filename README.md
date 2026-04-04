# Noctis Viewer v1.4.2

[English](README.md) | [简体中文](README.zh-CN.md) | [日本語](README.ja.md)

Noctis Viewer is a fast, minimal native image viewer for Windows, designed for quickly browsing PNG, JPG, and other common image formats with zero runtime dependencies.

It is especially useful for AI image workflows: when a PNG contains WebUI-compatible generation metadata, Noctis Viewer shows it in a clean two-column side panel while intentionally ignoring ComfyUI workflow JSON.

Starting with v1.3, Noctis Viewer also includes a powerful built-in HaldCLUT workflow for browsing, previewing, comparing, and exporting LUT-based looks in real time, making it a practical viewer for both prompt inspection and quick color grading review.

![Noctis Viewer preview](assets/preview.png)
![Noctis Viewer LUT preview](assets/preview_lut.png)

## Highlights

- Native Win32 + GDI+ application
- Lightweight and fast startup
- Full high-DPI display support (100% - 300% scaling)
- Browse PNG, JPG, JPEG, BMP, GIF, TIF, and TIFF files
- Arrow keys for previous / next image
- `Page Up` / `Page Down` for zoom in / out
- `Ctrl + Mouse wheel` for smooth zoom
- `Home` / `End` to jump to first / last image
- `Ctrl+O` to open file dialog
- Double-click the empty area to open an image
- Auto-fit to window on load and resize
- Real-time zoom level display in status bar
- Smooth mouse drag panning when zoomed
- Metadata side panel with click-to-copy values
- HaldCLUT panel with recursive directory scan
- `Original` entry to reset back to the unfiltered image
- **LUT intensity slider** (0-100%) for fine-tuning LUT strength
- Hold `Space` to preview the original image while a LUT is active
- HaldCLUT apply modes:
  - `MX_LUT Compatible`
  - `Smooth Interpolation`
- `File > Save Current Preview As...` to export the current preview
- File association support (add to "Open with" menu)
- Menu bar with File, View, Tools, and Help options
- Optimized rendering with double buffering to eliminate flickering

## New in v1.4.2

- **LUT Intensity Slider**: Adjust LUT strength from 0-100% in real-time
  - Located below "Original" entry in the LUT panel
  - Perfect for toning down aggressive LUTs (e.g., Bleach Bypass)
  - Smooth dragging with delayed preview for better performance
- **Fixed HaldCLUT Level Calculation**: Corrected level detection for 1728x1728 LUTs (Level 12)

## New in v1.4.1

- **Redesigned LUT Panel**: Simplified title, collapsible panel with matching style
- **Improved Rendering**: Fixed ghosting/artifacts when collapsing panels
- **Async LUT Selection**: Selection highlight appears immediately on click

## New in v1.4.0

- **High-DPI Display Support**: Per-monitor DPI awareness for crisp rendering on all displays
- **Ctrl + Mouse Wheel**: Smooth zoom in/out with Ctrl key modifier
- **Home / End Keys**: Quick navigation to first and last image
- **Improved Image Quality**: Better scaling with high-quality GDI+ rendering settings
- **Eliminated Panel Flickering**: Double buffering for smooth scrolling and dragging
- **Cached Backbuffer**: Hardware-accelerated smooth panning when zoomed
- **Async LUT Loading**: Responsive UI when selecting LUTs (selection updates immediately)

## New in v1.3.1

- Numeric keypad zoom support (`Num +` / `Num -`)
- Mouse drag panning when zoomed
- Fixed panel flickering during image drag
- Fixed HaldCLUT loading progress dialog

## Metadata Behavior

Noctis Viewer only shows WebUI-compatible metadata fields.

- Supported: `parameters`, plain prompt text
- Ignored on purpose: ComfyUI `prompt` / `workflow` JSON graphs

## HaldCLUT Credits

The HaldCLUT collection used during development and testing was sourced from:

https://github.com/cedeber/hald-clut

Many thanks to the `hald-clut` project and its contributors for providing and maintaining this excellent LUT collection.

## Controls

| Key | Action |
|-----|--------|
| `Left`, `Up` | Previous image |
| `Right`, `Down` | Next image |
| `Page Up`, `Num +` | Zoom in |
| `Page Down`, `Num -` | Zoom out |
| `Ctrl + Mouse wheel` | Zoom in / out |
| `Home` | Jump to first image |
| `End` | Jump to last image |
| `Ctrl+O` | Open file dialog |
| `Delete` | Delete current image (with confirmation) |
| `H` | Toggle HaldCLUT panel |
| `Space` (hold) | Preview original image while LUT is active |
| Mouse wheel | Previous / next image |
| Drag (when zoomed) | Pan the image |
| Double-click empty area | Open image |
| Click metadata value | Copy value to clipboard |
| Click HaldCLUT entry | Apply LUT |
| Click `Original` in HaldCLUT panel | Clear LUT selection |

## Build

### Requirements

- Windows 10 or later
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

- `Noctis_Viewer-v1.4.1-x64.zip`

Extract and run `Noctis_Viewer.exe`. No installation required.

## System Requirements

- Windows 10 or later
- High-DPI display support (100% - 300% scaling tested)
- No additional dependencies required

## GitHub

<https://github.com/aiimagestudio/NoctisViewer>

## License

This project is licensed under the MIT License. See [LICENSE](LICENSE).
