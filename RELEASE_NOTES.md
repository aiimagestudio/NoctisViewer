# Noctis Viewer v1.3.0

Noctis Viewer is a lightweight native Windows image viewer focused on fast browsing, clean metadata inspection, and real-time HaldCLUT color grading.

## What's New in v1.3.0

### New Features

- **HaldCLUT Mode Switching**
  - `MX_LUT Compatible` mode for ComfyUI-aligned results
  - `Smooth Interpolation` mode for smoother LUT previews

- **Original Entry in HaldCLUT Panel**
  - Fixed top-level `Original` item to return to the unfiltered image
  - Keeps the HaldCLUT panel open while clearing the current LUT

- **Save Current Preview**
  - Added `File > Save Current Preview As...`
  - Exports the currently displayed preview with or without LUT applied
  - Supports PNG, JPG, BMP, and TIFF output

### Fixes

- Fixed HaldCLUT application logic to match ComfyUI `MX_LUT` behavior
- Fixed HaldCLUT panel selection and reset behavior
- Fixed `Space` key conflict with the Generation Info header button
- Fixed HaldCLUT panel `[Configure]` click handling
- Removed temporary LUT debug text from the status bar

### Improvements

- Status bar now keeps normal file and zoom information while indicating LUT mode
- Added a higher-quality optional interpolated LUT mode
- Improved HaldCLUT usability for quick A/B comparison

## Download

- `Noctis_Viewer-v1.3.0-x64.zip`

## Previous Versions

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
