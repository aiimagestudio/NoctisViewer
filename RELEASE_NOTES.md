# Noctis Viewer v1.4.0

Noctis Viewer is a lightweight native Windows image viewer focused on fast browsing, clean metadata inspection, and real-time HaldCLUT color grading.

## What's New in v1.4.0

### New Features

- **High-DPI Display Support**
  - Added per-monitor DPI awareness for proper scaling on modern displays
  - All UI elements (panels, fonts, margins) now scale correctly with system DPI
  - Minimum window size scales with DPI to maintain usability

- **Enhanced Zoom Controls**
  - **Ctrl + Mouse Wheel**: Smooth zoom in/out at mouse position
  - Home/End keys: Jump to first/last image in folder

- **Improved Image Quality**
  - Fixed zoom-to-fit calculation to match ACDSee's percentage calculation
  - Added high-quality rendering settings:
    - `CompositingQualityHighQuality` for better alpha blending
    - `SmoothingModeHighQuality` for smoother edges
    - `UnitPixel` for precise 1:1 pixel mapping (no automatic DPI scaling)

### Performance & Visual Improvements

- **Drastically Reduced Flickering**
  - Implemented double buffering for Metadata and HaldCLUT panels
  - Added cached backbuffer system for smooth image panning when zoomed
  - Optimized redraw to only update necessary regions

- **Smoother Panning**
  - When zoomed, click and drag to pan the image with hardware-accelerated smoothness
  - Pan state cached in backbuffer for lag-free dragging

### Bug Fixes

- **Fixed LUT Panel Selection Delay**
  - Clicking a LUT entry now immediately shows the selection highlight
  - LUT file loading is properly deferred to prevent UI blocking
  - Uses timer-based async loading for responsive feedback

- **Fixed Fit-to-Window Zoom Calculation**
  - Removed incorrect margin subtraction that caused wrong zoom percentages
  - Now matches professional viewers (ACDSee) percentage display

### Under the Hood

- Added `GetDpiScale()` and `ScaleForDpi()` helper functions for consistent DPI handling
- Implemented `EnsureBackbuffer()` and `ClearBackbufferCache()` for optimized rendering
- All hardcoded sizes replaced with DPI-scaled equivalents

---

## Previous Versions

### v1.3.1

- **Numeric Keypad Zoom Support**: Added `Num +` / `Num -` keys for zooming
- **Mouse Drag Panning**: Click and drag to pan zoomed images
- **Fixed Panel Flickering During Drag**: Optimized redraw to prevent flicker
- **Fixed HaldCLUT Loading Progress Dialog**: Modal dialog improvements

### v1.3.0

- **HaldCLUT Mode Switching**: MX_LUT Compatible and Smooth Interpolation modes
- **Original Entry in HaldCLUT Panel**: Return to unfiltered image
- **Save Current Preview As**: Export with or without LUT applied

### v1.2.0

- HaldCLUT support with recursive scanning and async loading
- `config.ini` support for the HaldCLUT directory
- Progress dialog for HaldCLUT database loading

### v1.1.0

- Menu bar with File, Tools, and Help menus
- Zoom level display in status bar
- File association support

### v1.0.0

- Initial release with native Win32 + GDI+ image viewing
- WebUI-compatible PNG metadata panel

---

## Download

- `Noctis_Viewer-v1.4.0-x64.zip`

## System Requirements

- Windows 10 or later
- High-DPI display support (100% - 300% scaling tested)
- No additional dependencies required
