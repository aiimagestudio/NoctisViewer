# Noctis Viewer v1.1.0

Noctis Viewer is a lightweight native Windows image viewer focused on fast browsing and clean metadata inspection.

## What's New in v1.1.0

### New Features

- **Menu Bar**: Added File, Tools, and Help menus
  - File → Open (Ctrl+O)
  - Tools → Set as Default Image Viewer
  - Help → Visit GitHub
  - Help → About Noctis Viewer

- **Zoom Level Display**: Real-time zoom percentage shown in status bar
  - Updates when zooming with Page Up/Down
  - Updates when fitting to window
  - Only shows when an image is open

- **File Association**: Added ability to register with Windows
  - Adds Noctis Viewer to "Open with" menu for image files
  - Supports PNG, JPG, JPEG, BMP, GIF, TIFF
  - Accessible via Tools menu

- **About Dialog**: Shows version info and keyboard shortcuts
  - Includes GitHub repository link
  - One-click visit to GitHub

### Improvements

- Added Ctrl+O keyboard shortcut for opening files
- Fixed File menu Open command
- Improved menu command handling
- Various UI refinements

## System Requirements

- Windows 10/11 (x64)
- No additional runtime dependencies

## Download

- `Noctis_Viewer-v1.1.0-x64.zip`

Extract and run `Noctis_Viewer.exe`. No installation required.

## Previous Version: v1.0.0

Initial release with core features:

- Native Win32 + GDI+ application
- Fast startup and small footprint
- Dark UI with custom app icon
- Browse PNG, JPG, JPEG, BMP, GIF, TIF, and TIFF
- Arrow keys for previous / next image
- `Page Up` / `Page Down` zoom controls
- Auto-fit image on load and window resize
- WebUI-compatible PNG metadata panel
- Click metadata value cells to copy
- `Delete` key support with confirmation dialog
- Reduced paging flicker with double-buffered painting

## GitHub

https://github.com/aiimagestudio/NoctisViewer

## License

MIT License
