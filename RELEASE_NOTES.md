# Noctis Viewer v1.4.2

Noctis Viewer is a lightweight native Windows image viewer focused on fast browsing, clean metadata inspection, and real-time HaldCLUT color grading.

## What's New in v1.4.2

### New Features

- **LUT Intensity Slider**
  - Added intensity control (0-100%) in HaldCLUT panel
  - Located below "Original" entry when a LUT is selected
  - Real-time preview with smooth dragging
  - Perfect for toning down aggressive LUTs (e.g., Bleach Bypass)

### Bug Fixes

- **Fixed HaldCLUT Level Calculation**
  - Corrected level detection for 1728x1728 LUTs (Level 12)
  - Was incorrectly calculating as Level 144
  - Both MX and Smooth modes now work correctly

### Improvements

- **Smoother LUT Application**
  - Dragging intensity slider now uses delayed update (50ms)
  - Eliminates lag during rapid adjustments
  - Final value applied immediately on mouse release

---

## Previous Versions

### v1.4.1

#### UI Improvements

- **Redesigned LUT Panel**
  - Simplified title from "HaldCLUT Filters" to "LUT"
  - Title height now matches Info panel for visual consistency
  - Click the title bar to collapse/expand the panel
  - Collapsed width matches Info panel (120px)

- **Improved Button Labels**
  - Changed "[Configure]" to "Load" to prevent text truncation
  - Clearer indication of the button's purpose

#### Bug Fixes

- **Fixed Panel Rendering Issues**
  - Fixed ghosting/artifacts when collapsing panels
  - Fixed white blocks appearing during window resize
  - Full client area background erasure prevents visual glitches

- **Fixed LUT Selection Highlight Delay**
  - Selection highlight now appears immediately on click
  - LUT file loading properly deferred using PostMessage

### v1.4.0

#### New Features

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
    - `PixelOffsetModeHighQuality` for sub-pixel precision
  - Eliminated image shifting when zooming past 100%
  - Fixed single-pixel gaps when zooming above 400%

#### UI Improvements

- **Metadata Panel Enhancements**
  - Changed "Prompt" label to "Generation Info" for clarity
  - Added dark gray border line on the left edge
  - Key column now has distinct background color for better readability
  - Row height reduced to 28px for more compact display

- **Navigation Feedback**
  - Added visual feedback during image loading
  - Status bar updates immediately on navigation attempt

#### Bug Fixes

- **Image Loading**
  - Fixed intermittent blank window on first image load
  - GDI+ initialization now explicitly loads BMP, GIF, JPEG, PNG, and TIFF codecs
  - Ensures reliable image format support

- **Window State**
  - Fixed window opening at wrong position when previously closed while maximized
  - Now correctly restores non-maximized position before showing

- **Status Bar**
  - Fixed wrong format/size display when opening multiple files at once
  - Status bar now updates correctly after the first successful image load

- **Memory Management**
  - Fixed potential memory leak in metadata panel custom draw
  - Fixed potential crash when rapidly navigating images

### v1.3.1

#### New Features

- **LUT Modes**
  - Added two HaldCLUT application modes:
    - **MX_LUT Compatible**: Fast nearest-neighbor lookup
    - **Smooth Interpolation**: High-quality trilinear interpolation for smoother color transitions
  - Toggle via View → HaldCLUT Mode menu

#### UI Improvements

- **Better Default Panel State**
  - Metadata panel now starts collapsed to maximize image viewing area

#### Bug Fixes

- **DPI Scaling**
  - Fixed window size calculation on high-DPI displays
  - Status bar no longer cut off at high zoom levels

- **Performance**
  - Improved image navigation responsiveness

### v1.3.0

#### New Features

- **HaldCLUT Color Grading**
  - Real-time color lookup table (CLUT) application
  - Supports industry-standard HaldCLUT PNG files
  - Configurable directory for CLUT library
  - Organized by category folders
  - Hold Space to compare original vs graded image

#### UI Improvements

- **Dual Panel System**
  - Left panel: Image generation metadata (from PNG chunks)
  - Right panel: HaldCLUT filter selection
  - Both panels independently collapsible

- **Metadata Display**
  - Custom-drawn metadata panel with alternating row colors
  - Click to copy values to clipboard
  - Scrollable long content
  - Prompt parsing for WebUI-compatible PNG files

### v1.1.0

#### Core Features

- **Image Viewing**
  - Native Win32 + GDI+ for zero dependencies
  - Supports PNG, JPG, BMP, GIF, TIFF
  - Fast folder navigation with arrow keys and mouse wheel
  - Drag & drop support

- **Zoom & Pan**
  - Smooth zoom: Ctrl + Mouse Wheel, Page Up/Down, Num+/Num-
  - Click and drag to pan zoomed images
  - Zoom to fit with single key (Home)

- **File Operations**
  - Delete with confirmation (Delete key)
  - Copy to clipboard
  - Double-click empty area to open file dialog

- **UI Design**
  - Clean dark theme optimized for image viewing
  - Metadata panel for PNG generation parameters
  - Status bar with image dimensions, file size, zoom level
  - High-DPI aware display scaling

## License

MIT License - See LICENSE file for details
