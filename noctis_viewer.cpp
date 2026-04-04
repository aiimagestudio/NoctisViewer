#include <windows.h>
#include <commctrl.h>
#include <gdiplus.h>
#include "resource.h"
#include <shellapi.h>
#include <shlwapi.h>
#include <windowsx.h>
#include <shlobj.h>
#include <stdint.h>

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <thread>
#include <mutex>
#include <atomic>
#include <fstream>
#include <sstream>

using namespace Gdiplus;

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "shlwapi.lib")

namespace {

const wchar_t* kMainClassName = L"ComfyUIImageViewerNative";
const wchar_t* kMetadataViewClassName = L"ComfyUIMetadataView";
const wchar_t* kHaldCLUTViewClassName = L"NoctisHaldCLUTView";
const wchar_t* kProgressDialogClassName = L"NoctisProgressDialog";
const wchar_t* kWindowTitle = L"Noctis Viewer";
const wchar_t* kConfigFileName = L"config.ini";

constexpr int kMargin = 12;
constexpr int kStatusHeightFallback = 24;
constexpr int kPanelWidth = 280;  // Same width as HaldCLUT panel
constexpr int kHaldCLUTPanelWidth = 280;

// Menu IDs
constexpr UINT kMenuIdFileOpen = 2001;
constexpr UINT kMenuIdFileSavePreviewAs = 2002;
constexpr UINT kMenuIdToolsAssociate = 2101;
constexpr UINT kMenuIdViewHaldCLUT = 2150;
constexpr UINT kMenuIdViewHaldCLUTModeMx = 2151;
constexpr UINT kMenuIdViewHaldCLUTModeSmooth = 2152;
constexpr UINT kMenuIdHelpVisitGitHub = 2201;
constexpr UINT kMenuIdHelpAbout = 2202;

// Progress dialog IDs
constexpr UINT kProgressTimerId = 3001;
constexpr UINT kProgressUpdateMsg = WM_USER + 100;
constexpr UINT kLoadHaldCLUTMsg = WM_USER + 200;  // Async LUT loading message

// Version
constexpr wchar_t kAppVersion[] = L"1.4.0";
constexpr int kCollapsedPanelWidth = 120;
constexpr int kHeaderHeight = 34;
constexpr int kMinWindowWidth = 680;
constexpr int kMinWindowHeight = 480;
constexpr int kKeyColumnWidth = 120;
constexpr int kRowPaddingX = 10;
constexpr int kRowPaddingY = 8;
constexpr float kZoomStep = 1.2f;
constexpr float kMinZoom = 0.05f;
constexpr float kMaxZoom = 8.0f;

constexpr COLORREF kColorWindowBg = RGB(18, 18, 20);
constexpr COLORREF kColorCanvasBg = RGB(22, 22, 26);
constexpr COLORREF kColorPanelBg = RGB(28, 29, 34);
constexpr COLORREF kColorPanelRowA = RGB(33, 34, 40);
constexpr COLORREF kColorPanelRowB = RGB(38, 40, 46);
constexpr COLORREF kColorPanelHeader = RGB(44, 47, 56);
constexpr COLORREF kColorPanelHeaderHover = RGB(54, 58, 68);
constexpr COLORREF kColorBorder = RGB(74, 78, 92);
constexpr COLORREF kColorTextPrimary = RGB(230, 232, 236);
constexpr COLORREF kColorTextSecondary = RGB(170, 174, 182);
constexpr COLORREF kColorAccent = RGB(110, 168, 255);
constexpr COLORREF kColorProgressBar = RGB(70, 130, 220);

struct MetadataEntry {
    std::wstring key;
    std::wstring value;
    int height = 0;
};

// HaldCLUT structures
struct HaldCLUTEntry {
    std::wstring name;
    std::wstring path;
    int level = 0;
    int width = 0;
    int height = 0;
};

struct HaldCLUTCategory {
    std::wstring name;
    std::vector<HaldCLUTEntry> entries;
    bool expanded = true;
};

// HaldCLUT selection using indices instead of pointer
struct HaldCLUTSelection {
    int categoryIndex = -1;
    int entryIndex = -1;
    
    bool IsValid() const {
        return categoryIndex >= 0 && entryIndex >= 0;
    }
    
    void Reset() {
        categoryIndex = -1;
        entryIndex = -1;
    }
};

enum class HaldCLUTApplyMode {
    MxCompatible,
    SmoothInterpolated,
};

// Progress dialog data
struct LoadingProgress {
    std::atomic<int> current{0};
    std::atomic<int> total{0};
    std::atomic<bool> cancelled{false};
    std::atomic<bool> completed{false};
    std::wstring currentFile;
    std::mutex mutex;
};

HWND g_mainWindow = nullptr;
HWND g_statusBar = nullptr;
HWND g_metadataHeader = nullptr;
HWND g_metadataView = nullptr;
HWND g_haldCLUTPanel = nullptr;
HWND g_progressDialog = nullptr;

ULONG_PTR g_gdiplusToken = 0;
Image* g_currentImage = nullptr;
std::wstring g_currentFilePath;
std::vector<std::wstring> g_imageFiles;
std::vector<MetadataEntry> g_metadataEntries;

HFONT g_uiFont = nullptr;
HFONT g_uiFontBold = nullptr;
HBRUSH g_brushWindow = nullptr;
HBRUSH g_brushPanel = nullptr;
HBRUSH g_brushHeader = nullptr;
HBRUSH g_brushHeaderHover = nullptr;
HBRUSH g_brushStatus = nullptr;

int g_currentIndex = -1;
int g_metadataScrollPos = 0;
int g_metadataTotalHeight = 0;
float g_zoomLevel = 1.0f;
int g_panOffsetX = 0;  // Horizontal pan offset
int g_panOffsetY = 0;  // Vertical pan offset
bool g_isDragging = false;
POINT g_dragStartPos = {0, 0};
int g_dragStartPanX = 0;
int g_dragStartPanY = 0;

bool g_panelVisible = false;
bool g_panelCollapsed = false;
bool g_headerHot = false;

// HaldCLUT globals
bool g_haldCLUTPanelVisible = false;
bool g_haldCLUTHot = false;
int g_haldCLUTScrollPos = 0;
int g_haldCLUTTotalHeight = 0;
int g_haldCLUTHoverIndex = -1;
bool g_haldCLUTHoverOriginal = false;
std::vector<HaldCLUTCategory> g_haldCLUTCategories;
HaldCLUTSelection g_selectedHaldCLUT;
Image* g_haldCLUTImage = nullptr;
Bitmap* g_haldCLUTBitmap = nullptr;  // Cached as Bitmap for faster access
bool g_showingOriginal = false;
std::wstring g_haldCLUTBasePath;
HaldCLUTApplyMode g_haldCLUTApplyMode = HaldCLUTApplyMode::MxCompatible;

// DPI scaling helper
float GetDpiScale() {
    HWND hwnd = g_mainWindow ? g_mainWindow : GetDesktopWindow();
    UINT dpi = GetDpiForWindow(hwnd);
    if (dpi == 0) dpi = 96;
    return static_cast<float>(dpi) / 96.0f;
}

int ScaleForDpi(int value) {
    return static_cast<int>(value * GetDpiScale());
}

// Cached backbuffer for smooth dragging
HDC g_cachedMemoryDc = nullptr;
HBITMAP g_cachedBitmap = nullptr;
int g_cachedBufferWidth = 0;
int g_cachedBufferHeight = 0;

// Ensure the cached backbuffer matches the required size
void EnsureBackbuffer(int width, int height) {
    if (width <= 0 || height <= 0) return;
    
    if (g_cachedMemoryDc && g_cachedBitmap && 
        width == g_cachedBufferWidth && height == g_cachedBufferHeight) {
        return; // Buffer is already the right size
    }
    
    // Clean up old buffer
    if (g_cachedMemoryDc) {
        if (g_cachedBitmap) {
            SelectObject(g_cachedMemoryDc, g_cachedBitmap); // Deselect bitmap before deleting
            DeleteObject(g_cachedBitmap);
            g_cachedBitmap = nullptr;
        }
        DeleteDC(g_cachedMemoryDc);
        g_cachedMemoryDc = nullptr;
    }
    
    // Create new buffer
    HDC screenDc = GetDC(nullptr);
    g_cachedMemoryDc = CreateCompatibleDC(screenDc);
    g_cachedBitmap = CreateCompatibleBitmap(screenDc, width, height);
    SelectObject(g_cachedMemoryDc, g_cachedBitmap);
    g_cachedBufferWidth = width;
    g_cachedBufferHeight = height;
    ReleaseDC(nullptr, screenDc);
}

void ClearBackbufferCache() {
    if (g_cachedMemoryDc) {
        if (g_cachedBitmap) {
            DeleteObject(g_cachedBitmap);
            g_cachedBitmap = nullptr;
        }
        DeleteDC(g_cachedMemoryDc);
        g_cachedMemoryDc = nullptr;
    }
    g_cachedBufferWidth = 0;
    g_cachedBufferHeight = 0;
}

LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK MetadataViewProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK HaldCLUTViewProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK ProgressDialogProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
void ClearViewerState();
bool DeleteCurrentImage();
void CreateMainMenu(HWND hwnd);
bool RegisterFileAssociation(const wchar_t* ext, bool setAsDefault);
void ShowAssociationDialog(HWND hwnd);

// HaldCLUT functions
int CalculateHaldLevel(int width, int height);
bool ValidateHaldCLUT(const std::wstring& path, int& outLevel, int& outWidth, int& outHeight);
void LoadHaldCLUTDirectoryRecursive(const std::wstring& path, const std::wstring& categoryName, 
                                     LoadingProgress* progress = nullptr);
void LoadHaldCLUTDatabaseAsync();
void ToggleHaldCLUTPanel();
void ApplyHaldCLUTToBitmap(Bitmap* source, Bitmap* clut, int level);
void ApplyHaldCLUTToBitmapMx(Bitmap* source, Bitmap* clut);
void ApplyHaldCLUTToBitmapSmooth(Bitmap* source, Bitmap* clut, int level);
void UpdateHaldCLUTLayout();
void LoadSelectedHaldCLUT();
void ClearLoadedHaldCLUT();
void UpdateMenuState();
bool BuildPreviewBitmap(Bitmap& outBitmap);
bool SaveCurrentPreviewAs(HWND hwnd);
bool GetEncoderClsid(const WCHAR* mimeType, CLSID* pClsid);

// Config functions
std::wstring GetConfigPath();
void LoadConfig();
void SaveConfig();
void ShowConfigDialog(HWND hwnd);

// Progress dialog
void ShowLoadingProgressDialog(LoadingProgress* progress);
void CloseLoadingProgressDialog();

std::wstring ToLower(const std::wstring& input) {
    std::wstring result = input;
    std::transform(result.begin(), result.end(), result.begin(), towlower);
    return result;
}

std::wstring Trim(const std::wstring& input) {
    const size_t start = input.find_first_not_of(L" \t\r\n");
    if (start == std::wstring::npos) {
        return L"";
    }
    const size_t end = input.find_last_not_of(L" \t\r\n");
    return input.substr(start, end - start + 1);
}

std::wstring GetFileNameOnly(const std::wstring& path) {
    const size_t pos = path.find_last_of(L"\\/");
    return pos == std::wstring::npos ? path : path.substr(pos + 1);
}

std::wstring GetDirectoryName(const std::wstring& path) {
    const size_t pos = path.find_last_of(L"\\/");
    return pos == std::wstring::npos ? L"" : path.substr(0, pos);
}

bool EndsWith(const std::wstring& value, const wchar_t* suffix) {
    const size_t suffixLen = wcslen(suffix);
    return value.size() >= suffixLen &&
           value.compare(value.size() - suffixLen, suffixLen, suffix) == 0;
}

bool IsSupportedImageFile(const std::wstring& path) {
    const std::wstring lower = ToLower(path);
    return EndsWith(lower, L".png") || EndsWith(lower, L".jpg") || EndsWith(lower, L".jpeg") ||
           EndsWith(lower, L".bmp") || EndsWith(lower, L".gif") || EndsWith(lower, L".tif") ||
           EndsWith(lower, L".tiff");
}

void ShowError(const wchar_t* message) {
    MessageBoxW(g_mainWindow, message, L"Error", MB_OK | MB_ICONERROR);
}

void CopyTextToClipboard(const std::wstring& text) {
    if (text.empty() || !OpenClipboard(g_mainWindow)) {
        return;
    }

    EmptyClipboard();
    const size_t bytes = (text.size() + 1) * sizeof(wchar_t);
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (memory) {
        void* target = GlobalLock(memory);
        if (target) {
            memcpy(target, text.c_str(), bytes);
            GlobalUnlock(memory);
            SetClipboardData(CF_UNICODETEXT, memory);
        } else {
            GlobalFree(memory);
        }
    }
    CloseClipboard();
}

// Get the full path of the executable
std::wstring GetExecutablePath() {
    wchar_t path[MAX_PATH];
    if (GetModuleFileNameW(nullptr, path, MAX_PATH)) {
        return std::wstring(path);
    }
    return L"";
}

// Get the directory of the executable
std::wstring GetExecutableDirectory() {
    std::wstring exePath = GetExecutablePath();
    size_t pos = exePath.find_last_of(L"\\/");
    return pos == std::wstring::npos ? L"" : exePath.substr(0, pos);
}

// Config file path
std::wstring GetConfigPath() {
    return GetExecutableDirectory() + L"\\" + kConfigFileName;
}

// Load configuration from INI file
void LoadConfig() {
    std::wstring configPath = GetConfigPath();
    wchar_t pathBuffer[MAX_PATH] = {0};
    
    GetPrivateProfileStringW(L"HaldCLUT", L"Path", L"", pathBuffer, MAX_PATH, configPath.c_str());
    
    if (wcslen(pathBuffer) > 0) {
        g_haldCLUTBasePath = pathBuffer;
    } else {
        // Default empty - user must configure
        g_haldCLUTBasePath = L"";
    }
}

// Save configuration to INI file
void SaveConfig() {
    std::wstring configPath = GetConfigPath();
    WritePrivateProfileStringW(L"HaldCLUT", L"Path", g_haldCLUTBasePath.c_str(), configPath.c_str());
}

// Show configuration dialog
void ShowConfigDialog(HWND hwnd) {
    // Create a simple input dialog
    wchar_t pathBuffer[MAX_PATH] = {0};
    wcsncpy_s(pathBuffer, g_haldCLUTBasePath.c_str(), MAX_PATH - 1);
    
    // Simple dialog using Windows API
    // For simplicity, use SHBrowseForFolder or a simple input box
    BROWSEINFOW bi = {};
    bi.hwndOwner = hwnd;
    bi.lpszTitle = L"Select HaldCLUT Directory:";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    
    LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
    if (pidl) {
        wchar_t selectedPath[MAX_PATH];
        if (SHGetPathFromIDListW(pidl, selectedPath)) {
            g_haldCLUTBasePath = selectedPath;
            SaveConfig();
            
            // Reload if panel is visible
            if (g_haldCLUTPanelVisible) {
                LoadHaldCLUTDatabaseAsync();
            }
        }
        CoTaskMemFree(pidl);
    }
}

// Register file association for current user (no admin required)
bool RegisterFileAssociationForUser(const wchar_t* ext, const wchar_t* progId, bool setAsDefault) {
    std::wstring exePath = GetExecutablePath();
    if (exePath.empty()) {
        return false;
    }

    // Build command line: "path" "%1"
    std::wstring commandLine = L"\"" + exePath + L"\" \"%1\"";
    
    HKEY hKey;
    LONG result;

    // 1. Register under Software\Classes\Applications\Noctis_Viewer.exe
    std::wstring appKey = L"Software\\Classes\\Applications\\Noctis_Viewer.exe\\shell\\open\\command";
    result = RegCreateKeyExW(HKEY_CURRENT_USER, appKey.c_str(), 0, nullptr, 0, KEY_WRITE, nullptr, &hKey, nullptr);
    if (result == ERROR_SUCCESS) {
        RegSetValueExW(hKey, nullptr, 0, REG_SZ, (BYTE*)commandLine.c_str(), static_cast<DWORD>((commandLine.length() + 1) * sizeof(wchar_t)));
        RegCloseKey(hKey);
    }

    // 2. Add to OpenWithList for this extension
    std::wstring openWithKey = std::wstring(L"Software\\Classes\\") + ext + L"\\OpenWithList\\Noctis_Viewer.exe";
    result = RegCreateKeyExW(HKEY_CURRENT_USER, openWithKey.c_str(), 0, nullptr, 0, KEY_WRITE, nullptr, &hKey, nullptr);
    if (result == ERROR_SUCCESS) {
        RegCloseKey(hKey);
    }

    // 3. Create ProgID for this extension (optional, for friendly name and icon)
    std::wstring progIdKey = std::wstring(L"Software\\Classes\\") + progId;
    result = RegCreateKeyExW(HKEY_CURRENT_USER, progIdKey.c_str(), 0, nullptr, 0, KEY_WRITE, nullptr, &hKey, nullptr);
    if (result == ERROR_SUCCESS) {
        std::wstring friendlyName = L"Image File (Noctis Viewer)";
        RegSetValueExW(hKey, nullptr, 0, REG_SZ, (BYTE*)friendlyName.c_str(), static_cast<DWORD>((friendlyName.length() + 1) * sizeof(wchar_t)));
        RegCloseKey(hKey);

        // Set command
        std::wstring progIdCmdKey = progIdKey + L"\\shell\\open\\command";
        result = RegCreateKeyExW(HKEY_CURRENT_USER, progIdCmdKey.c_str(), 0, nullptr, 0, KEY_WRITE, nullptr, &hKey, nullptr);
        if (result == ERROR_SUCCESS) {
            RegSetValueExW(hKey, nullptr, 0, REG_SZ, (BYTE*)commandLine.c_str(), static_cast<DWORD>((commandLine.length() + 1) * sizeof(wchar_t)));
            RegCloseKey(hKey);
        }

        // Set default icon
        std::wstring progIdIconKey = progIdKey + L"\\DefaultIcon";
        result = RegCreateKeyExW(HKEY_CURRENT_USER, progIdIconKey.c_str(), 0, nullptr, 0, KEY_WRITE, nullptr, &hKey, nullptr);
        if (result == ERROR_SUCCESS) {
            std::wstring iconPath = exePath + L",0";
            RegSetValueExW(hKey, nullptr, 0, REG_SZ, (BYTE*)iconPath.c_str(), static_cast<DWORD>((iconPath.length() + 1) * sizeof(wchar_t)));
            RegCloseKey(hKey);
        }
    }

    // 4. Set as default if requested
    if (setAsDefault) {
        std::wstring extKey = std::wstring(L"Software\\Classes\\") + ext;
        result = RegCreateKeyExW(HKEY_CURRENT_USER, extKey.c_str(), 0, nullptr, 0, KEY_WRITE, nullptr, &hKey, nullptr);
        if (result == ERROR_SUCCESS) {
            RegSetValueExW(hKey, nullptr, 0, REG_SZ, (BYTE*)progId, static_cast<DWORD>((wcslen(progId) + 1) * sizeof(wchar_t)));
            RegCloseKey(hKey);
        }
    }

    // Notify Windows of the change
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    return true;
}

// Register file association (public function)
bool RegisterFileAssociation(const wchar_t* ext, bool setAsDefault) {
    std::wstring progId = std::wstring(L"NoctisViewer.") + ext;
    return RegisterFileAssociationForUser(ext, progId.c_str(), setAsDefault);
}

// Show association dialog
void ShowAssociationDialog(HWND hwnd) {
    // First, register to OpenWithList
    (void)RegisterFileAssociation(L".png", false);
    (void)RegisterFileAssociation(L".jpg", false);
    (void)RegisterFileAssociation(L".jpeg", false);
    (void)RegisterFileAssociation(L".bmp", false);
    (void)RegisterFileAssociation(L".gif", false);
    (void)RegisterFileAssociation(L".tiff", false);
    
    const wchar_t* message = L"Noctis Viewer has been added to the 'Open with' menu.\n\n"
                             L"To set it as the default program for image files, "
                             L"Windows will open the Default Apps settings.\n\n"
                             L"Click OK to open settings (you'll need to manually select Noctis Viewer), "
                             L"or Cancel to skip.";
    
    int result = MessageBoxW(hwnd, message, L"File Association", MB_OKCANCEL | MB_ICONINFORMATION);
    
    if (result == IDOK) {
        // Open Windows Settings -> Default Apps
        ShellExecuteW(hwnd, L"open", L"ms-settings:defaultapps", nullptr, nullptr, SW_SHOWNORMAL);
    }
}

// Create main menu
void CreateMainMenu(HWND hwnd) {
    HMENU hMenu = CreateMenu();

    // File menu
    HMENU hFileMenu = CreatePopupMenu();
    AppendMenuW(hFileMenu, MF_STRING, kMenuIdFileOpen, L"&Open...\tCtrl+O");
    AppendMenuW(hFileMenu, MF_STRING, kMenuIdFileSavePreviewAs, L"&Save Current Preview As...");
    AppendMenuW(hFileMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hFileMenu, MF_STRING, SC_CLOSE, L"E&xit\tAlt+F4");
    AppendMenuW(hMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(hFileMenu), L"&File");

    // View menu
    HMENU hViewMenu = CreatePopupMenu();
    AppendMenuW(hViewMenu, MF_STRING, kMenuIdViewHaldCLUT, L"&HaldCLUT Panel\tH");
    HMENU hHaldModeMenu = CreatePopupMenu();
    AppendMenuW(hHaldModeMenu, MF_STRING, kMenuIdViewHaldCLUTModeMx, L"&MX_LUT Compatible");
    AppendMenuW(hHaldModeMenu, MF_STRING, kMenuIdViewHaldCLUTModeSmooth, L"&Smooth Interpolation");
    AppendMenuW(hViewMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(hHaldModeMenu), L"HaldCLUT &Mode");
    AppendMenuW(hMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(hViewMenu), L"&View");

    // Tools menu
    HMENU hToolsMenu = CreatePopupMenu();
    AppendMenuW(hToolsMenu, MF_STRING, kMenuIdToolsAssociate, L"&Set as Default Image Viewer...");
    AppendMenuW(hMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(hToolsMenu), L"&Tools");

    // Help menu
    HMENU hHelpMenu = CreatePopupMenu();
    AppendMenuW(hHelpMenu, MF_STRING, kMenuIdHelpVisitGitHub, L"&Visit GitHub");
    AppendMenuW(hHelpMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hHelpMenu, MF_STRING, kMenuIdHelpAbout, L"&About Noctis Viewer");
    AppendMenuW(hMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(hHelpMenu), L"&Help");

    SetMenu(hwnd, hMenu);
    UpdateMenuState();
    DrawMenuBar(hwnd);
}

std::wstring DecodeUtf8OrAnsi(const std::vector<uint8_t>& bytes) {
    if (bytes.empty()) {
        return L"";
    }

    const auto decode = [&](UINT codePage) -> std::wstring {
        const int size = MultiByteToWideChar(codePage, 0, reinterpret_cast<const char*>(bytes.data()),
                                             static_cast<int>(bytes.size()), nullptr, 0);
        if (size <= 0) {
            return L"";
        }
        std::wstring result(size, L'\0');
        MultiByteToWideChar(codePage, 0, reinterpret_cast<const char*>(bytes.data()),
                            static_cast<int>(bytes.size()), &result[0], size);
        return result;
    };

    std::wstring text = decode(CP_UTF8);
    if (!text.empty()) {
        return text;
    }
    return decode(CP_ACP);
}

bool LooksLikeWorkflowJson(const std::wstring& value) {
    const std::wstring trimmed = Trim(value);
    if (trimmed.size() < 2 || trimmed.front() != L'{' || trimmed.back() != L'}') {
        return false;
    }
    return trimmed.find(L"\"class_type\"") != std::wstring::npos ||
           trimmed.find(L"\"inputs\"") != std::wstring::npos ||
           trimmed.find(L"\"_meta\"") != std::wstring::npos;
}

std::vector<std::wstring> SplitLines(const std::wstring& text) {
    std::vector<std::wstring> lines;
    std::wstring current;
    for (wchar_t ch : text) {
        if (ch == L'\r') {
            continue;
        }
        if (ch == L'\n') {
            lines.push_back(current);
            current.clear();
        } else {
            current.push_back(ch);
        }
    }
    lines.push_back(current);
    return lines;
}

std::vector<std::wstring> SplitParameterTokens(const std::wstring& text) {
    std::vector<std::wstring> tokens;
    std::wstring current;
    int parenDepth = 0;
    int bracketDepth = 0;
    int braceDepth = 0;
    bool inQuote = false;

    for (size_t i = 0; i < text.size(); ++i) {
        const wchar_t ch = text[i];
        if (ch == L'"') {
            inQuote = !inQuote;
        } else if (!inQuote) {
            if (ch == L'(') {
                ++parenDepth;
            } else if (ch == L')' && parenDepth > 0) {
                --parenDepth;
            } else if (ch == L'[') {
                ++bracketDepth;
            } else if (ch == L']' && bracketDepth > 0) {
                --bracketDepth;
            } else if (ch == L'{') {
                ++braceDepth;
            } else if (ch == L'}' && braceDepth > 0) {
                --braceDepth;
            }
        }

        if (ch == L',' && !inQuote && parenDepth == 0 && bracketDepth == 0 && braceDepth == 0) {
            const std::wstring token = Trim(current);
            if (!token.empty()) {
                tokens.push_back(token);
            }
            current.clear();
        } else {
            current.push_back(ch);
        }
    }

    const std::wstring tail = Trim(current);
    if (!tail.empty()) {
        tokens.push_back(tail);
    }
    return tokens;
}

std::vector<MetadataEntry> ParseWebUiParameters(const std::wstring& parametersText, const std::wstring& plainPrompt) {
    std::vector<MetadataEntry> entries;
    std::wstring promptText = plainPrompt;
    std::wstring parameterLine;

    if (!parametersText.empty()) {
        const std::vector<std::wstring> lines = SplitLines(parametersText);
        std::vector<std::wstring> promptLines;
        std::vector<std::wstring> parameterLines;
        bool inParameterSection = false;

        for (const std::wstring& rawLine : lines) {
            const std::wstring line = Trim(rawLine);
            if (line.empty()) {
                continue;
            }

            const bool looksLikeParams =
                line.find(L"Steps:") != std::wstring::npos ||
                line.find(L"Sampler:") != std::wstring::npos ||
                line.find(L"Seed:") != std::wstring::npos ||
                line.find(L"CFG scale:") != std::wstring::npos;

            if (looksLikeParams || inParameterSection) {
                inParameterSection = true;
                parameterLines.push_back(line);
            } else {
                promptLines.push_back(line);
            }
        }

        if (!promptLines.empty()) {
            promptText.clear();
            for (size_t i = 0; i < promptLines.size(); ++i) {
                if (i > 0) {
                    promptText += L"\n";
                }
                promptText += promptLines[i];
            }
        }

        for (size_t i = 0; i < parameterLines.size(); ++i) {
            if (i > 0) {
                parameterLine += L", ";
            }
            parameterLine += parameterLines[i];
        }
    }

    std::wstring positivePrompt = Trim(promptText);
    std::wstring negativePrompt;
    const size_t negativePos = positivePrompt.find(L"Negative prompt:");
    if (negativePos != std::wstring::npos) {
        negativePrompt = Trim(positivePrompt.substr(negativePos + 16));
        positivePrompt = Trim(positivePrompt.substr(0, negativePos));
    }

    if (!positivePrompt.empty()) {
        entries.push_back({L"Prompt", positivePrompt, 0});
    }
    if (!negativePrompt.empty()) {
        entries.push_back({L"Negative Prompt", negativePrompt, 0});
    }

    for (const std::wstring& token : SplitParameterTokens(parameterLine)) {
        const size_t colon = token.find(L':');
        if (colon == std::wstring::npos) {
            continue;
        }
        const std::wstring key = Trim(token.substr(0, colon));
        const std::wstring value = Trim(token.substr(colon + 1));
        if (!key.empty() && !value.empty()) {
            entries.push_back({key, value, 0});
        }
    }

    if (entries.empty() && !plainPrompt.empty()) {
        entries.push_back({L"Prompt", Trim(plainPrompt), 0});
    }
    return entries;
}

uint32_t ReadBigEndian32(const uint8_t* bytes) {
    return (static_cast<uint32_t>(bytes[0]) << 24) |
           (static_cast<uint32_t>(bytes[1]) << 16) |
           (static_cast<uint32_t>(bytes[2]) << 8) |
           static_cast<uint32_t>(bytes[3]);
}

bool ReadPngTextChunk(FILE* file, std::wstring& parameters, std::wstring& plainPrompt) {
    uint8_t signature[8];
    if (fread(signature, 1, sizeof(signature), file) != sizeof(signature)) {
        return false;
    }

    static const uint8_t kPngSignature[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    if (memcmp(signature, kPngSignature, sizeof(kPngSignature)) != 0) {
        return false;
    }

    while (true) {
        uint8_t lengthBytes[4];
        if (fread(lengthBytes, 1, sizeof(lengthBytes), file) != sizeof(lengthBytes)) {
            break;
        }

        const uint32_t chunkLength = ReadBigEndian32(lengthBytes);
        // Sanity check: PNG chunks shouldn't be larger than 10MB
        if (chunkLength > 10 * 1024 * 1024) {
            break;
        }
        
        char chunkType[5] = {};
        if (fread(chunkType, 1, 4, file) != 4) {
            break;
        }

        std::vector<uint8_t> chunkData(chunkLength);
        if (chunkLength > 0 && fread(chunkData.data(), 1, chunkLength, file) != chunkLength) {
            break;
        }

        uint8_t crc[4];
        if (fread(crc, 1, sizeof(crc), file) != sizeof(crc)) {
            break;
        }

        const std::string type(chunkType, chunkType + 4);
        std::string keyword;
        std::vector<uint8_t> textBytes;

        if (type == "tEXt") {
            const auto nullPos = std::find(chunkData.begin(), chunkData.end(), 0);
            if (nullPos != chunkData.end()) {
                keyword.assign(chunkData.begin(), nullPos);
                textBytes.assign(nullPos + 1, chunkData.end());
            }
        } else if (type == "iTXt") {
            size_t pos = 0;
            while (pos < chunkData.size() && chunkData[pos] != 0) {
                keyword.push_back(static_cast<char>(chunkData[pos++]));
            }
            if (pos >= chunkData.size()) {
                continue;
            }
            ++pos;
            if (pos + 1 >= chunkData.size()) {
                continue;
            }
            const uint8_t compressionFlag = chunkData[pos++];
            ++pos;
            while (pos < chunkData.size() && chunkData[pos] != 0) {
                ++pos;
            }
            if (pos < chunkData.size()) {
                ++pos;
            }
            while (pos < chunkData.size() && chunkData[pos] != 0) {
                ++pos;
            }
            if (pos < chunkData.size()) {
                ++pos;
            }
            // Fix: Add boundary check before accessing chunkData
            if (compressionFlag == 0 && pos <= chunkData.size()) {
                textBytes.assign(chunkData.begin() + static_cast<std::ptrdiff_t>(pos), chunkData.end());
            }
        }

        if (!keyword.empty() && !textBytes.empty()) {
            std::vector<uint8_t> keywordBytes(keyword.begin(), keyword.end());
            std::wstring key = Trim(DecodeUtf8OrAnsi(keywordBytes));
            std::wstring value = Trim(DecodeUtf8OrAnsi(textBytes));

            if ((key == L"parameters" || key == L"Parameters") && !LooksLikeWorkflowJson(value)) {
                parameters = value;
            } else if ((key == L"prompt" || key == L"Prompt") && !LooksLikeWorkflowJson(value)) {
                plainPrompt = value;
            }
        }

        if (type == "IEND") {
            break;
        }
    }

    return !parameters.empty() || !plainPrompt.empty();
}

std::vector<MetadataEntry> ExtractMetadataEntries(const std::wstring& filePath) {
    if (!EndsWith(ToLower(filePath), L".png")) {
        return {};
    }

    FILE* file = nullptr;
    _wfopen_s(&file, filePath.c_str(), L"rb");
    if (!file) {
        return {};
    }

    std::wstring parameters;
    std::wstring plainPrompt;
    const bool found = ReadPngTextChunk(file, parameters, plainPrompt);
    fclose(file);

    if (!found) {
        return {};
    }

    return ParseWebUiParameters(parameters, plainPrompt);
}

int GetStatusBarHeight() {
    if (g_statusBar) {
        RECT rc;
        if (GetClientRect(g_statusBar, &rc)) {
            int h = rc.bottom - rc.top;
            if (h > 0) return h;
        }
    }
    return kStatusHeightFallback;
}

int GetCurrentPanelWidth() {
    float dpiScale = GetDpiScale();
    int width = 0;
    if (g_panelVisible) {
        int panelWidth = g_panelCollapsed ? kCollapsedPanelWidth : kPanelWidth;
        width += static_cast<int>(panelWidth * dpiScale);
    }
    if (g_haldCLUTPanelVisible) {
        width += static_cast<int>(kHaldCLUTPanelWidth * dpiScale);
    }
    return width;
}

RECT GetImageViewportRect() {
    RECT client{};
    GetClientRect(g_mainWindow, &client);
    client.bottom -= GetStatusBarHeight();
    client.right -= GetCurrentPanelWidth();
    return client;
}

void UpdateStatusBar() {
    std::wstring text;
    if (!g_currentFilePath.empty() && g_currentIndex >= 0 && !g_imageFiles.empty()) {
        text = GetFileNameOnly(g_currentFilePath) + L" | " + GetDirectoryName(g_currentFilePath) + L" (" +
               std::to_wstring(g_currentIndex + 1) + L"/" + std::to_wstring(g_imageFiles.size()) + L")";
        text += L" | " + std::to_wstring(static_cast<int>(g_zoomLevel * 100)) + L"%";
        if (g_selectedHaldCLUT.IsValid() && g_haldCLUTBitmap && !g_showingOriginal) {
            text += (g_haldCLUTApplyMode == HaldCLUTApplyMode::MxCompatible)
                ? L" | LUT: MX"
                : L" | LUT: Smooth";
        }
    } else {
        text = L"Double-click to open | Arrow keys/Mouse wheel navigate | Num +/- zoom | Drag to pan"; 
    }
    SetWindowTextW(g_statusBar, text.c_str());
}

void UpdateMenuState() {
    if (!g_mainWindow) return;
    HMENU menu = GetMenu(g_mainWindow);
    if (!menu) return;

    CheckMenuRadioItem(menu, kMenuIdViewHaldCLUTModeMx, kMenuIdViewHaldCLUTModeSmooth,
                       g_haldCLUTApplyMode == HaldCLUTApplyMode::MxCompatible
                           ? kMenuIdViewHaldCLUTModeMx
                           : kMenuIdViewHaldCLUTModeSmooth,
                       MF_BYCOMMAND);

    const UINT saveFlags = (g_currentImage != nullptr) ? MF_BYCOMMAND | MF_ENABLED : MF_BYCOMMAND | MF_GRAYED;
    EnableMenuItem(menu, kMenuIdFileSavePreviewAs, saveFlags);
    DrawMenuBar(g_mainWindow);
}

void RecalculateMetadataLayout() {
    if (!g_metadataView) {
        return;
    }

    RECT rc{};
    GetClientRect(g_metadataView, &rc);
    const int contentWidth = std::max<int>(40, rc.right - rc.left - 1);
    const int keyWidth = std::min(kKeyColumnWidth, std::max(80, contentWidth / 3));
    const int valueWidth = std::max(40, contentWidth - keyWidth - 1);

    HDC dc = GetDC(g_metadataView);
    HFONT oldFont = static_cast<HFONT>(SelectObject(dc, g_uiFont));

    g_metadataTotalHeight = 0;
    for (MetadataEntry& entry : g_metadataEntries) {
        RECT keyRect = {0, 0, keyWidth - 2 * kRowPaddingX, 0};
        RECT valueRect = {0, 0, valueWidth - 2 * kRowPaddingX, 0};

        DrawTextW(dc, entry.key.c_str(), -1, &keyRect, DT_CALCRECT | DT_WORDBREAK | DT_EDITCONTROL);
        DrawTextW(dc, entry.value.c_str(), -1, &valueRect, DT_CALCRECT | DT_WORDBREAK | DT_EDITCONTROL);

        const int contentHeight = std::max(keyRect.bottom - keyRect.top, valueRect.bottom - valueRect.top);
        entry.height = std::max(28, contentHeight + 2 * kRowPaddingY);
        g_metadataTotalHeight += entry.height;
    }

    SelectObject(dc, oldFont);
    ReleaseDC(g_metadataView, dc);

    const int pageHeight = std::max<int>(0, rc.bottom - rc.top);
    SCROLLINFO si{};
    si.cbSize = sizeof(si);
    si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
    si.nMin = 0;
    si.nMax = std::max(0, g_metadataTotalHeight - 1);
    si.nPage = static_cast<UINT>(pageHeight);
    si.nPos = std::clamp(g_metadataScrollPos, 0, std::max(0, g_metadataTotalHeight - pageHeight));
    g_metadataScrollPos = si.nPos;
    SetScrollInfo(g_metadataView, SB_VERT, &si, TRUE);
}

void RefreshMetadataPanel() {
    g_panelVisible = !g_metadataEntries.empty();
    if (!g_panelVisible) {
        g_panelCollapsed = false;
    }
    g_metadataScrollPos = 0;

    if (g_metadataHeader) {
        const wchar_t* text = L"Info";
        SetWindowTextW(g_metadataHeader, text);
        ShowWindow(g_metadataHeader, g_panelVisible ? SW_SHOW : SW_HIDE);
    }

    if (g_metadataView) {
        bool shouldShow = g_panelVisible && !g_panelCollapsed;
        ShowWindow(g_metadataView, shouldShow ? SW_SHOW : SW_HIDE);
        if (shouldShow) {
            RecalculateMetadataLayout();
            // Force complete redraw with background erase to prevent ghosting
            InvalidateRect(g_metadataView, nullptr, TRUE);
            UpdateWindow(g_metadataView);
        }
    }
}

void UpdateWindowTitle() {
    std::wstring title = kWindowTitle;
    if (!g_currentFilePath.empty()) {
        title += L" - ";
        title += GetFileNameOnly(g_currentFilePath);
    }
    SetWindowTextW(g_mainWindow, title.c_str());
}

void LayoutChildren(bool invalidateMainWindow = true) {
    RECT client{};
    GetClientRect(g_mainWindow, &client);

    const int statusHeight = GetStatusBarHeight();
    const int contentHeight = std::max<int>(0, client.bottom - statusHeight);
    float dpiScale = GetDpiScale();

    // Calculate positions from right to left
    int rightEdge = client.right;

    // HaldCLUT panel is rightmost
    if (g_haldCLUTPanelVisible && g_haldCLUTPanel) {
        int haldWidth = static_cast<int>(kHaldCLUTPanelWidth * dpiScale);
        MoveWindow(g_haldCLUTPanel, rightEdge - haldWidth, 0, haldWidth, contentHeight, TRUE);
        ShowWindow(g_haldCLUTPanel, SW_SHOW);
        rightEdge -= haldWidth;
    } else if (g_haldCLUTPanel) {
        ShowWindow(g_haldCLUTPanel, SW_HIDE);
    }

    // Metadata panel is to the left of HaldCLUT
    if (g_panelVisible) {
        int metaWidth = static_cast<int>((g_panelCollapsed ? kCollapsedPanelWidth : kPanelWidth) * dpiScale);
        int headerHeight = static_cast<int>(kHeaderHeight * dpiScale);
        if (g_metadataHeader) {
            MoveWindow(g_metadataHeader, rightEdge - metaWidth, 0, metaWidth, headerHeight, TRUE);
            ShowWindow(g_metadataHeader, SW_SHOW);
        }
        if (g_metadataView) {
            bool wasVisible = IsWindowVisible(g_metadataView);
            bool shouldShow = !g_panelCollapsed;
            
            if (shouldShow) {
                MoveWindow(g_metadataView, rightEdge - metaWidth, headerHeight, metaWidth,
                           std::max(0, contentHeight - headerHeight), TRUE);
            }
            ShowWindow(g_metadataView, shouldShow ? SW_SHOW : SW_HIDE);
            
            // When hiding the panel, force main window repaint of that area to clear ghosting
            if (wasVisible && !shouldShow && g_mainWindow) {
                RECT oldPanelArea{rightEdge - metaWidth, headerHeight, rightEdge, contentHeight};
                InvalidateRect(g_mainWindow, &oldPanelArea, TRUE);
            }
            // Force complete redraw when showing to prevent ghosting
            if (shouldShow) {
                InvalidateRect(g_metadataView, nullptr, TRUE);
            }
        }
        rightEdge -= metaWidth;
    } else {
        if (g_metadataHeader) ShowWindow(g_metadataHeader, SW_HIDE);
        if (g_metadataView) ShowWindow(g_metadataView, SW_HIDE);
    }

    if (g_statusBar) {
        MoveWindow(g_statusBar, 0, client.bottom - statusHeight, client.right, statusHeight, TRUE);
    }

    if (g_panelVisible && !g_panelCollapsed) {
        RecalculateMetadataLayout();
    }

    if (g_haldCLUTPanelVisible) {
        UpdateHaldCLUTLayout();
    }

    if (invalidateMainWindow) {
        InvalidateRect(g_mainWindow, nullptr, FALSE);
    }
}

void FitToWindow() {
    if (!g_currentImage) {
        return;
    }

    const RECT viewport = GetImageViewportRect();
    const int viewWidth = std::max<int>(1, viewport.right - viewport.left - 2 * kMargin);
    const int viewHeight = std::max<int>(1, viewport.bottom - viewport.top - 2 * kMargin);

    const int imageWidth = static_cast<int>(g_currentImage->GetWidth());
    const int imageHeight = static_cast<int>(g_currentImage->GetHeight());
    if (imageWidth <= 0 || imageHeight <= 0) {
        return;
    }

    const float scaleX = static_cast<float>(viewWidth) / static_cast<float>(imageWidth);
    const float scaleY = static_cast<float>(viewHeight) / static_cast<float>(imageHeight);
    g_zoomLevel = std::clamp(std::min(scaleX, scaleY), kMinZoom, 1.0f);
    g_panOffsetX = 0;
    g_panOffsetY = 0;
    UpdateStatusBar();
}

std::wstring NormalizePathSeparator(const std::wstring& path) {
    if (path.empty()) return path;
    std::wstring result = path;
    while (!result.empty() && (result.back() == L'\\' || result.back() == L'/')) {
        result.pop_back();
    }
    return result;
}

void LoadFolderImages(const std::wstring& folderPath) {
    g_imageFiles.clear();
    WIN32_FIND_DATAW findData{};

    std::wstring normalizedPath = NormalizePathSeparator(folderPath);
    std::wstring searchPath = normalizedPath + L"\\*";

    HANDLE handle = FindFirstFileW(searchPath.c_str(), &findData);
    if (handle == INVALID_HANDLE_VALUE) {
        return;
    }

    do {
        if ((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            continue;
        }

        const std::wstring name = findData.cFileName;
        if (!IsSupportedImageFile(name)) {
            continue;
        }

        g_imageFiles.push_back(normalizedPath + L"\\" + name);
    } while (FindNextFileW(handle, &findData));

    FindClose(handle);
    std::sort(g_imageFiles.begin(), g_imageFiles.end());
}

void UpdateCurrentIndex() {
    g_currentIndex = -1;
    for (size_t i = 0; i < g_imageFiles.size(); ++i) {
        if (_wcsicmp(g_imageFiles[i].c_str(), g_currentFilePath.c_str()) == 0) {
            g_currentIndex = static_cast<int>(i);
            break;
        }
    }
}

void SetCurrentImage(Image* image) {
    if (g_currentImage) {
        delete g_currentImage;
        g_currentImage = nullptr;
    }
    g_currentImage = image;
    UpdateMenuState();
}

void ClearViewerState() {
    SetCurrentImage(nullptr);
    g_currentFilePath.clear();
    g_imageFiles.clear();
    g_metadataEntries.clear();
    g_currentIndex = -1;
    g_metadataScrollPos = 0;
    g_zoomLevel = 1.0f;
    g_panOffsetX = 0;
    g_panOffsetY = 0;
    g_showingOriginal = false;
    g_selectedHaldCLUT.Reset();
    ClearLoadedHaldCLUT();
    RefreshMetadataPanel();
    UpdateWindowTitle();
    UpdateStatusBar();
    LayoutChildren(false);
    InvalidateRect(g_mainWindow, nullptr, FALSE);
}

bool LoadImageFile(const std::wstring& filePath, bool preserveViewState = false) {
    Image* image = Image::FromFile(filePath.c_str(), FALSE);
    if (!image || image->GetLastStatus() != Ok) {
        if (image) {
            delete image;
        }
        ShowError(L"Failed to load the image.");
        return false;
    }

    SetCurrentImage(image);
    g_currentFilePath = filePath;
    LoadFolderImages(GetDirectoryName(filePath));
    UpdateCurrentIndex();
    g_metadataEntries = ExtractMetadataEntries(filePath);
    RefreshMetadataPanel();
    UpdateWindowTitle();
    UpdateStatusBar();
    LayoutChildren(false);
    
    if (preserveViewState) {
        // Keep current zoom and pan when navigating between images
        // Just invalidate the image viewport to avoid full redraw flicker
        RECT viewport = GetImageViewportRect();
        InvalidateRect(g_mainWindow, &viewport, FALSE);
    } else {
        FitToWindow();
        InvalidateRect(g_mainWindow, nullptr, FALSE);
    }
    return true;
}

bool DeleteCurrentImage() {
    if (g_currentFilePath.empty() || g_currentIndex < 0) {
        return false;
    }

    const std::wstring fileToDelete = g_currentFilePath;
    const std::wstring fileName = GetFileNameOnly(fileToDelete);
    const std::wstring prompt =
        L"Delete this image?\n\n" + fileName + L"\n\nThis will permanently delete the file.";

    const int answer = MessageBoxW(
        g_mainWindow,
        prompt.c_str(),
        L"Confirm Delete",
        MB_ICONWARNING | MB_YESNO | MB_DEFBUTTON2);
    if (answer != IDYES) {
        return false;
    }

    int fallbackIndex = -1;
    if (g_currentIndex + 1 < static_cast<int>(g_imageFiles.size())) {
        fallbackIndex = g_currentIndex + 1;
    } else if (g_currentIndex - 1 >= 0) {
        fallbackIndex = g_currentIndex - 1;
    }

    SetCurrentImage(nullptr);

    if (!DeleteFileW(fileToDelete.c_str())) {
        ShowError(L"Failed to delete the image file.");
        LoadImageFile(fileToDelete);
        return false;
    }

    LoadFolderImages(GetDirectoryName(fileToDelete));
    if (g_imageFiles.empty() || fallbackIndex < 0) {
        ClearViewerState();
        return true;
    }

    if (fallbackIndex >= static_cast<int>(g_imageFiles.size())) {
        fallbackIndex = static_cast<int>(g_imageFiles.size()) - 1;
    }

    return LoadImageFile(g_imageFiles[fallbackIndex]);
}

void NavigateRelative(int offset) {
    if (g_imageFiles.empty() || g_currentIndex < 0) {
        return;
    }
    const int nextIndex = g_currentIndex + offset;
    if (nextIndex < 0 || nextIndex >= static_cast<int>(g_imageFiles.size())) {
        return;
    }
    // Preserve zoom and pan state when navigating between images to reduce flicker
    LoadImageFile(g_imageFiles[nextIndex], true);
}

void NavigateToFirst() {
    if (g_imageFiles.empty()) {
        return;
    }
    LoadImageFile(g_imageFiles[0], true);
}

void NavigateToLast() {
    if (g_imageFiles.empty()) {
        return;
    }
    LoadImageFile(g_imageFiles[g_imageFiles.size() - 1], true);
}

void OpenImageDialog(HWND owner) {
    OPENFILENAMEW ofn{};
    wchar_t filePath[MAX_PATH] = L"";
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = owner;
    ofn.lpstrFilter =
        L"Image Files\0*.png;*.jpg;*.jpeg;*.bmp;*.gif;*.tif;*.tiff\0All Files\0*.*\0";
    ofn.lpstrFile = filePath;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

    if (GetOpenFileNameW(&ofn)) {
        LoadImageFile(filePath);
    }
}

void ZoomIn() {
    if (!g_currentImage) {
        return;
    }
    g_zoomLevel = std::min(g_zoomLevel * kZoomStep, kMaxZoom);
    UpdateStatusBar();
    InvalidateRect(g_mainWindow, nullptr, FALSE);
}

void ZoomOut() {
    if (!g_currentImage) {
        return;
    }
    g_zoomLevel = std::max(g_zoomLevel / kZoomStep, kMinZoom);
    UpdateStatusBar();
    InvalidateRect(g_mainWindow, nullptr, FALSE);
}

void ScrollMetadata(int delta) {
    if (!g_metadataView || g_metadataEntries.empty()) {
        return;
    }

    RECT rc{};
    GetClientRect(g_metadataView, &rc);
    const int pageHeight = std::max<int>(0, rc.bottom - rc.top);
    const int maxPos = std::max<int>(0, g_metadataTotalHeight - pageHeight);
    g_metadataScrollPos = std::clamp(g_metadataScrollPos + delta, 0, maxPos);

    SCROLLINFO si{};
    si.cbSize = sizeof(si);
    si.fMask = SIF_POS;
    si.nPos = g_metadataScrollPos;
    SetScrollInfo(g_metadataView, SB_VERT, &si, TRUE);
    InvalidateRect(g_metadataView, nullptr, FALSE);
}

void PaintImage(HDC dc) {
    RECT viewport = GetImageViewportRect();
    HBRUSH bgBrush = CreateSolidBrush(kColorCanvasBg);
    FillRect(dc, &viewport, bgBrush);
    DeleteObject(bgBrush);

    if (!g_currentImage) {
        SetBkMode(dc, TRANSPARENT);
        SelectObject(dc, g_uiFont);
        SetTextColor(dc, kColorTextSecondary);
        DrawTextW(dc, L"Double-click the empty area to choose an image", -1, &viewport,
                  DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        return;
    }

    const int imageWidth = static_cast<int>(g_currentImage->GetWidth());
    const int imageHeight = static_cast<int>(g_currentImage->GetHeight());
    const int drawWidth = std::max(1, static_cast<int>(imageWidth * g_zoomLevel));
    const int drawHeight = std::max(1, static_cast<int>(imageHeight * g_zoomLevel));
    const int viewWidth = viewport.right - viewport.left;
    const int viewHeight = viewport.bottom - viewport.top;

    // Calculate base position (centered)
    int baseX = viewport.left + (viewWidth - drawWidth) / 2;
    int baseY = viewport.top + (viewHeight - drawHeight) / 2;
    
    // Apply pan offset
    int x = baseX + g_panOffsetX;
    int y = baseY + g_panOffsetY;
    
    // Clamp pan offset to keep image visible (allow some overscroll)
    const int maxOverscroll = 50;
    if (drawWidth > viewWidth) {
        // Image wider than view: allow scrolling left/right
        int minX = viewport.left + viewWidth - drawWidth - maxOverscroll;
        int maxX = viewport.left + maxOverscroll;
        x = std::clamp(x, minX, maxX);
    } else {
        // Image fits width: center it
        x = baseX;
    }
    if (drawHeight > viewHeight) {
        // Image taller than view: allow scrolling up/down
        int minY = viewport.top + viewHeight - drawHeight - maxOverscroll;
        int maxY = viewport.top + maxOverscroll;
        y = std::clamp(y, minY, maxY);
    } else {
        // Image fits height: center it
        y = baseY;
    }

    // Check if we need to apply HaldCLUT
    bool shouldApplyCLUT = (g_selectedHaldCLUT.IsValid() && g_haldCLUTBitmap && !g_showingOriginal);

    if (shouldApplyCLUT) {
        // Create temp bitmap at display size to avoid double resampling
        Bitmap tempBitmap(drawWidth, drawHeight, PixelFormat32bppARGB);
        if (tempBitmap.GetLastStatus() == Ok) {
            // First, draw and scale the image to target size
            Graphics scaleGraphics(&tempBitmap);
            scaleGraphics.SetInterpolationMode(InterpolationModeHighQualityBicubic);
            scaleGraphics.SetPixelOffsetMode(PixelOffsetModeHighQuality);
            scaleGraphics.DrawImage(g_currentImage, 0, 0, drawWidth, drawHeight);

            // Then apply HaldCLUT at display resolution
            HaldCLUTCategory& cat = g_haldCLUTCategories[g_selectedHaldCLUT.categoryIndex];
            HaldCLUTEntry& entry = cat.entries[g_selectedHaldCLUT.entryIndex];
            ApplyHaldCLUTToBitmap(&tempBitmap, g_haldCLUTBitmap, entry.level);

            // Draw the result directly (no further scaling)
            Graphics graphics(dc);
            graphics.SetInterpolationMode(InterpolationModeHighQualityBicubic);
            graphics.SetPixelOffsetMode(PixelOffsetModeHighQuality);
            graphics.DrawImage(&tempBitmap, x, y, drawWidth, drawHeight);
            
            // Draw LUT name overlay at top-left of the image
            HaldCLUTCategory& displayCat = g_haldCLUTCategories[g_selectedHaldCLUT.categoryIndex];
            HaldCLUTEntry& displayEntry = displayCat.entries[g_selectedHaldCLUT.entryIndex];
            std::wstring lutName = displayEntry.name;
            
            // Use GDI (not GDI+) for text with background
            int textX = x + 10;
            int textY = y + 10;
            
            // Calculate text size first
            RECT textRect = {0, 0, 0, 0};
            DrawTextW(dc, lutName.c_str(), -1, &textRect, DT_CALCRECT | DT_SINGLELINE);
            int textWidth = textRect.right - textRect.left;
            int textHeight = textRect.bottom - textRect.top;
            
            // Draw semi-transparent background
            RECT bgRect = {textX - 4, textY - 2, textX + textWidth + 8, textY + textHeight + 4};
            COLORREF oldTextColor = SetTextColor(dc, RGB(255, 255, 255));
            int oldBkMode = SetBkMode(dc, OPAQUE);
            COLORREF oldBkColor = SetBkColor(dc, RGB(0, 0, 0));
            
            // Draw with a slight shadow effect
            SetTextColor(dc, RGB(0, 0, 0));
            RECT shadowRect = {textX + 1, textY + 1, textX + textWidth + 1, textY + textHeight + 1};
            DrawTextW(dc, lutName.c_str(), -1, &shadowRect, DT_SINGLELINE);
            
            SetTextColor(dc, RGB(255, 255, 255));
            RECT finalRect = {textX, textY, textX + textWidth, textY + textHeight};
            DrawTextW(dc, lutName.c_str(), -1, &finalRect, DT_SINGLELINE);
            
            SetTextColor(dc, oldTextColor);
            SetBkMode(dc, oldBkMode);
            SetBkColor(dc, oldBkColor);
        } else {
            // Fallback if temp bitmap creation fails
            Graphics graphics(dc);
            graphics.SetInterpolationMode(InterpolationModeHighQualityBicubic);
            graphics.SetPixelOffsetMode(PixelOffsetModeHighQuality);
            graphics.DrawImage(g_currentImage, x, y, drawWidth, drawHeight);
        }
    } else {
        Graphics graphics(dc);
        graphics.SetInterpolationMode(InterpolationModeHighQualityBicubic);
        graphics.SetPixelOffsetMode(PixelOffsetModeHighQuality);
        graphics.SetCompositingQuality(CompositingQualityHighQuality);
        graphics.SetSmoothingMode(SmoothingModeHighQuality);
        // Disable automatic DPI scaling to ensure 1:1 pixel mapping
        graphics.SetPageUnit(UnitPixel);
        graphics.DrawImage(g_currentImage, x, y, drawWidth, drawHeight);
    }
}

bool BuildPreviewBitmap(Bitmap& outBitmap) {
    if (!g_currentImage) {
        return false;
    }

    const int imageWidth = static_cast<int>(g_currentImage->GetWidth());
    const int imageHeight = static_cast<int>(g_currentImage->GetHeight());
    Graphics graphics(&outBitmap);
    if (graphics.GetLastStatus() != Ok) {
        return false;
    }

    graphics.DrawImage(g_currentImage, 0, 0, imageWidth, imageHeight);
    if (graphics.GetLastStatus() != Ok) {
        return false;
    }

    const bool shouldApplyCLUT = (g_selectedHaldCLUT.IsValid() && g_haldCLUTBitmap && !g_showingOriginal);
    if (!shouldApplyCLUT) {
        return true;
    }

    HaldCLUTCategory& cat = g_haldCLUTCategories[g_selectedHaldCLUT.categoryIndex];
    HaldCLUTEntry& entry = cat.entries[g_selectedHaldCLUT.entryIndex];
    ApplyHaldCLUTToBitmap(&outBitmap, g_haldCLUTBitmap, entry.level);
    return true;
}

bool GetEncoderClsid(const WCHAR* mimeType, CLSID* pClsid) {
    UINT num = 0;
    UINT size = 0;
    GetImageEncodersSize(&num, &size);
    if (size == 0) {
        return false;
    }

    std::vector<BYTE> buffer(size);
    ImageCodecInfo* codecs = reinterpret_cast<ImageCodecInfo*>(buffer.data());
    if (GetImageEncoders(num, size, codecs) != Ok) {
        return false;
    }

    for (UINT i = 0; i < num; ++i) {
        if (wcscmp(codecs[i].MimeType, mimeType) == 0) {
            *pClsid = codecs[i].Clsid;
            return true;
        }
    }
    return false;
}

bool SaveCurrentPreviewAs(HWND hwnd) {
    if (!g_currentImage) {
        return false;
    }

    const std::wstring baseName = g_currentFilePath.empty()
        ? L"preview"
        : GetFileNameOnly(g_currentFilePath).substr(0, GetFileNameOnly(g_currentFilePath).find_last_of(L'.'));
    const std::wstring suffix = (g_selectedHaldCLUT.IsValid() && g_haldCLUTBitmap && !g_showingOriginal) ? L"_lut" : L"_preview";
    std::wstring defaultName = baseName + suffix + L".png";

    wchar_t fileName[MAX_PATH] = L"";
    wcsncpy_s(fileName, defaultName.c_str(), _TRUNCATE);

    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = L"PNG Image (*.png)\0*.png\0JPEG Image (*.jpg)\0*.jpg\0BMP Image (*.bmp)\0*.bmp\0TIFF Image (*.tif)\0*.tif\0\0";
    ofn.lpstrFile = fileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrDefExt = L"png";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    ofn.lpstrTitle = L"Save Current Preview As";

    if (!GetSaveFileNameW(&ofn)) {
        return false;
    }

    const int imageWidth = static_cast<int>(g_currentImage->GetWidth());
    const int imageHeight = static_cast<int>(g_currentImage->GetHeight());
    Bitmap previewBitmap(imageWidth, imageHeight, PixelFormat32bppARGB);
    if (previewBitmap.GetLastStatus() != Ok || !BuildPreviewBitmap(previewBitmap)) {
        ShowError(L"Failed to prepare the preview image for saving.");
        return false;
    }

    std::wstring lowerPath = ToLower(fileName);
    const wchar_t* mimeType = L"image/png";
    if (EndsWith(lowerPath, L".jpg") || EndsWith(lowerPath, L".jpeg")) {
        mimeType = L"image/jpeg";
    } else if (EndsWith(lowerPath, L".bmp")) {
        mimeType = L"image/bmp";
    } else if (EndsWith(lowerPath, L".tif") || EndsWith(lowerPath, L".tiff")) {
        mimeType = L"image/tiff";
    }

    CLSID encoderClsid{};
    if (!GetEncoderClsid(mimeType, &encoderClsid)) {
        ShowError(L"Failed to find a matching image encoder.");
        return false;
    }

    if (previewBitmap.Save(fileName, &encoderClsid, nullptr) != Ok) {
        ShowError(L"Failed to save the preview image.");
        return false;
    }

    SetWindowTextW(g_statusBar, (std::wstring(L"Saved preview: ") + fileName).c_str());
    return true;
}

LRESULT CALLBACK MetadataViewProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_SIZE:
        RecalculateMetadataLayout();
        return 0;

    case WM_MOUSEWHEEL:
        ScrollMetadata(-(GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA) * 48);
        return 0;

    case WM_VSCROLL: {
        SCROLLINFO si{};
        si.cbSize = sizeof(si);
        si.fMask = SIF_ALL;
        GetScrollInfo(hwnd, SB_VERT, &si);

        int newPos = si.nPos;
        switch (LOWORD(wParam)) {
        case SB_LINEUP:
            newPos -= 48;
            break;
        case SB_LINEDOWN:
            newPos += 48;
            break;
        case SB_PAGEUP:
            newPos -= static_cast<int>(si.nPage);
            break;
        case SB_PAGEDOWN:
            newPos += static_cast<int>(si.nPage);
            break;
        case SB_THUMBTRACK:
        case SB_THUMBPOSITION:
            newPos = si.nTrackPos;
            break;
        default:
            break;
        }

        g_metadataScrollPos = std::clamp(newPos, si.nMin,
                                         std::max(si.nMin, si.nMax - static_cast<int>(si.nPage) + 1));
        si.fMask = SIF_POS;
        si.nPos = g_metadataScrollPos;
        SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }

    case WM_ERASEBKGND:
        return 1;

    case WM_LBUTTONUP: {
        RECT client{};
        GetClientRect(hwnd, &client);
        const int width = client.right - client.left;
        const int keyWidth = std::min(kKeyColumnWidth, std::max(80, width / 3));
        const int x = GET_X_LPARAM(lParam);
        const int y = GET_Y_LPARAM(lParam) + g_metadataScrollPos;

        if (x >= keyWidth) {
            int top = 0;
            for (const MetadataEntry& entry : g_metadataEntries) {
                if (y >= top && y < top + entry.height) {
                    CopyTextToClipboard(entry.value);
                    const std::wstring status = L"Copied value for " + entry.key;
                    SetWindowTextW(g_statusBar, status.c_str());
                    return 0;
                }
                top += entry.height;
            }
        }
        break;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps{};
        HDC dc = BeginPaint(hwnd, &ps);

        RECT client{};
        GetClientRect(hwnd, &client);
        int width = client.right - client.left;
        int height = client.bottom - client.top;
        
        // Double buffering to prevent flickering during scroll
        HDC memoryDc = CreateCompatibleDC(dc);
        HBITMAP backBuffer = CreateCompatibleBitmap(dc, width, height);
        HBITMAP oldBitmap = static_cast<HBITMAP>(SelectObject(memoryDc, backBuffer));

        FillRect(memoryDc, &client, g_brushPanel);

        const int contentWidth = width;
        const int keyWidth = std::min(kKeyColumnWidth, std::max(80, contentWidth / 3));

        SetBkMode(memoryDc, TRANSPARENT);
        HFONT oldFont = static_cast<HFONT>(SelectObject(memoryDc, g_uiFont));

        int y = -g_metadataScrollPos;
        for (size_t i = 0; i < g_metadataEntries.size(); ++i) {
            const MetadataEntry& entry = g_metadataEntries[i];
            RECT rowRect = {0, y, contentWidth, y + entry.height};
            if (rowRect.bottom >= 0 && rowRect.top <= height) {
                HBRUSH rowBrush = CreateSolidBrush((i % 2 == 0) ? kColorPanelRowA : kColorPanelRowB);
                FillRect(memoryDc, &rowRect, rowBrush);
                DeleteObject(rowBrush);

                RECT keyRect = {rowRect.left + kRowPaddingX, rowRect.top + kRowPaddingY,
                                rowRect.left + keyWidth - kRowPaddingX, rowRect.bottom - kRowPaddingY};
                RECT valueRect = {rowRect.left + keyWidth + kRowPaddingX, rowRect.top + kRowPaddingY,
                                  rowRect.right - kRowPaddingX, rowRect.bottom - kRowPaddingY};

                SelectObject(memoryDc, g_uiFontBold);
                SetTextColor(memoryDc, kColorTextSecondary);
                DrawTextW(memoryDc, entry.key.c_str(), -1, &keyRect, DT_WORDBREAK | DT_EDITCONTROL);

                SelectObject(memoryDc, g_uiFont);
                SetTextColor(memoryDc, kColorTextPrimary);
                DrawTextW(memoryDc, entry.value.c_str(), -1, &valueRect, DT_WORDBREAK | DT_EDITCONTROL);

                HPEN pen = CreatePen(PS_SOLID, 1, kColorBorder);
                HPEN oldPen = static_cast<HPEN>(SelectObject(memoryDc, pen));
                MoveToEx(memoryDc, keyWidth, rowRect.top, nullptr);
                LineTo(memoryDc, keyWidth, rowRect.bottom);
                MoveToEx(memoryDc, rowRect.left, rowRect.bottom - 1, nullptr);
                LineTo(memoryDc, rowRect.right, rowRect.bottom - 1);
                SelectObject(memoryDc, oldPen);
                DeleteObject(pen);
            }
            y += entry.height;
        }

        HPEN borderPen = CreatePen(PS_SOLID, 1, kColorBorder);
        HPEN oldBorderPen = static_cast<HPEN>(SelectObject(memoryDc, borderPen));
        HGDIOBJ oldBrush = SelectObject(memoryDc, GetStockObject(NULL_BRUSH));
        Rectangle(memoryDc, client.left, client.top, client.right, client.bottom);
        SelectObject(memoryDc, oldBrush);
        SelectObject(memoryDc, oldBorderPen);
        DeleteObject(borderPen);

        SelectObject(memoryDc, oldFont);
        
        // Copy to screen
        BitBlt(dc, 0, 0, width, height, memoryDc, 0, 0, SRCCOPY);
        
        SelectObject(memoryDc, oldBitmap);
        DeleteObject(backBuffer);
        DeleteDC(memoryDc);
        
        EndPaint(hwnd, &ps);
        return 0;
    }
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

// ==================== HaldCLUT Implementation ====================

// HaldCLUT: image_size^2 = level^3
// e.g., level=64 -> image=512x512 -> 512^2 = 262144 = 64^3
// Fix: Add overflow protection
int CalculateHaldLevel(int width, int height) {
    if (width != height) return 0;
    int size = width;
    int level = 1;
    // Maximum reasonable level (level^3 <= INT_MAX, so level <= 1625)
    const int maxLevel = 1625;
    while (level <= maxLevel && level * level * level < size * size) { 
        ++level; 
    }
    if (level > maxLevel || level * level * level != size * size) return 0;
    return level;
}

bool ValidateHaldCLUT(const std::wstring& path, int& outLevel, int& outWidth, int& outHeight) {
    // Quick file size check first to avoid loading huge non-HaldCLUT files
    WIN32_FIND_DATAW findData;
    HANDLE hFind = FindFirstFileW(path.c_str(), &findData);
    if (hFind == INVALID_HANDLE_VALUE) {
        return false;
    }
    FindClose(hFind);
    
    // HaldCLUT files are typically PNGs of reasonable size (few MB)
    // Skip files that are obviously not HaldCLUTs (too large)
    if (findData.nFileSizeHigh > 0 || findData.nFileSizeLow > 50 * 1024 * 1024) {
        return false;  // Skip files > 50MB
    }
    
    Image* img = Image::FromFile(path.c_str(), FALSE);
    if (!img || img->GetLastStatus() != Ok) {
        if (img) delete img;
        return false;
    }
    int w = static_cast<int>(img->GetWidth());
    int h = static_cast<int>(img->GetHeight());
    delete img;

    int level = CalculateHaldLevel(w, h);
    if (level <= 0) return false;

    outLevel = level;
    outWidth = w;
    outHeight = h;
    return true;
}

// Fix: Optimized recursive directory loading with progress tracking
void LoadHaldCLUTDirectoryRecursive(const std::wstring& path, const std::wstring& categoryName,
                                     LoadingProgress* progress) {
    HaldCLUTCategory category;
    category.name = categoryName;
    category.expanded = true;

    WIN32_FIND_DATAW findData{};
    std::wstring normalizedPath = NormalizePathSeparator(path);
    std::wstring searchPath = normalizedPath + L"\\*";

    HANDLE hFind = FindFirstFileW(searchPath.c_str(), &findData);

    if (hFind == INVALID_HANDLE_VALUE) {
        return;
    }

    std::vector<std::wstring> subdirs;
    std::vector<std::wstring> files;

    do {
        if (progress && progress->cancelled.load()) {
            FindClose(hFind);
            return;
        }

        const std::wstring name = findData.cFileName;
        if (name == L"." || name == L"..") continue;

        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            subdirs.push_back(name);
        } else if (EndsWith(ToLower(name), L".png")) {
            files.push_back(name);
        }
    } while (FindNextFileW(hFind, &findData));

    FindClose(hFind);

    // Sort for consistent ordering
    std::sort(files.begin(), files.end());
    std::sort(subdirs.begin(), subdirs.end());

    // Update total count for progress
    if (progress) {
        progress->total += static_cast<int>(files.size());
    }

    // Add PNG files to current category
    for (const auto& file : files) {
        if (progress && progress->cancelled.load()) {
            return;
        }

        std::wstring fullPath = normalizedPath + L"\\" + file;

        if (progress) {
            std::lock_guard<std::mutex> lock(progress->mutex);
            progress->currentFile = file;
            progress->current++;
        }

        int level, width, height;
        if (ValidateHaldCLUT(fullPath, level, width, height)) {
            HaldCLUTEntry entry;
            entry.name = file.substr(0, file.length() - 4);  // Remove .png
            entry.path = fullPath;
            entry.level = level;
            entry.width = width;
            entry.height = height;
            category.entries.push_back(entry);
        }
    }

    // Add category if it has entries
    if (!category.entries.empty()) {
        g_haldCLUTCategories.push_back(category);
    }

    // Recurse into subdirectories
    for (const auto& subdir : subdirs) {
        if (progress && progress->cancelled.load()) {
            return;
        }
        std::wstring subPath = normalizedPath + L"\\" + subdir;
        std::wstring subCategoryName = categoryName.empty() ? subdir : categoryName + L" / " + subdir;
        LoadHaldCLUTDirectoryRecursive(subPath, subCategoryName, progress);
    }
}

// Progress dialog procedure
LRESULT CALLBACK ProgressDialogProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    static LoadingProgress* s_progress = nullptr;
    
    switch (msg) {
    case WM_CREATE: {
        CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        s_progress = reinterpret_cast<LoadingProgress*>(cs->lpCreateParams);
        
        // Get DPI scale for proper sizing
        float dpiScale = GetDpiScale();
        int margin = static_cast<int>(20 * dpiScale);
        int dlgWidth = static_cast<int>(400 * dpiScale);
        int btnWidth = static_cast<int>(80 * dpiScale);
        int btnHeight = static_cast<int>(25 * dpiScale);
        int progressHeight = static_cast<int>(20 * dpiScale);
        int textHeight = static_cast<int>(30 * dpiScale);
        
        // Create progress bar
        HWND hProgress = CreateWindowExW(0, PROGRESS_CLASSW, nullptr,
            WS_CHILD | WS_VISIBLE | PBS_SMOOTH,
            margin, static_cast<int>(60 * dpiScale), dlgWidth - 2 * margin, progressHeight, 
            hwnd, reinterpret_cast<HMENU>(1), nullptr, nullptr);
        SendMessageW(hProgress, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
        SendMessageW(hProgress, PBM_SETPOS, 0, 0);
        
        // Create cancel button (centered)
        CreateWindowExW(0, L"BUTTON", L"Cancel",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            (dlgWidth - btnWidth) / 2, static_cast<int>(100 * dpiScale), btnWidth, btnHeight, 
            hwnd, reinterpret_cast<HMENU>(2), nullptr, nullptr);
        
        // Create status text
        CreateWindowExW(0, L"STATIC", L"Scanning HaldCLUT files...",
            WS_CHILD | WS_VISIBLE | SS_CENTER,
            margin, margin, dlgWidth - 2 * margin, textHeight, 
            hwnd, reinterpret_cast<HMENU>(3), nullptr, nullptr);
        
        // Start timer for updates
        SetTimer(hwnd, kProgressTimerId, 50, nullptr);
        return 0;
    }
    
    case WM_TIMER:
        if (wParam == kProgressTimerId && s_progress) {
            if (s_progress->completed.load() || s_progress->cancelled.load()) {
                DestroyWindow(hwnd);
                return 0;
            }
            
            HWND hProgress = GetDlgItem(hwnd, 1);
            HWND hStatus = GetDlgItem(hwnd, 3);
            
            int total = s_progress->total.load();
            int current = s_progress->current.load();
            
            if (total > 0) {
                int percent = (current * 100) / total;
                SendMessageW(hProgress, PBM_SETPOS, percent, 0);
            }
            
            std::wstring status;
            {
                std::lock_guard<std::mutex> lock(s_progress->mutex);
                status = L"Loading: " + s_progress->currentFile + L"\n(" + 
                         std::to_wstring(current) + L"/" + std::to_wstring(total) + L")";
            }
            SetWindowTextW(hStatus, status.c_str());
        }
        return 0;
    
    case WM_COMMAND:
        if (LOWORD(wParam) == 2 && s_progress) {  // Cancel button
            s_progress->cancelled.store(true);
            return 0;
        }
        break;
        
    case WM_CLOSE:
        if (s_progress) {
            s_progress->cancelled.store(true);
        }
        DestroyWindow(hwnd);
        return 0;
        
    case WM_DESTROY:
        KillTimer(hwnd, kProgressTimerId);
        return 0;
    }
    
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void ShowLoadingProgressDialog(LoadingProgress* progress) {
    if (!g_mainWindow) return;
    
    // Get DPI scale for proper sizing
    float dpiScale = GetDpiScale();
    int dlgWidth = static_cast<int>(400 * dpiScale);
    int dlgHeight = static_cast<int>(180 * dpiScale);
    
    // Center on parent
    RECT rcParent;
    GetWindowRect(g_mainWindow, &rcParent);
    int x = rcParent.left + (rcParent.right - rcParent.left - dlgWidth) / 2;
    int y = rcParent.top + (rcParent.bottom - rcParent.top - dlgHeight) / 2;
    
    // Create a modal-style progress dialog with TOPMOST to ensure it's visible
    g_progressDialog = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_DLGMODALFRAME, kProgressDialogClassName,
        L"Loading HaldCLUT Database",
        WS_VISIBLE | WS_POPUP | WS_CAPTION | WS_SYSMENU,
        x, y, dlgWidth, dlgHeight,
        g_mainWindow, nullptr, 
        reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(g_mainWindow, GWLP_HINSTANCE)),
        progress);
    
    // Disable main window to create modal behavior
    EnableWindow(g_mainWindow, FALSE);
    
    // Force the dialog to show and activate
    if (g_progressDialog) {
        ShowWindow(g_progressDialog, SW_SHOW);
        SetForegroundWindow(g_progressDialog);
        SetActiveWindow(g_progressDialog);
        // Force immediate paint
        UpdateWindow(g_progressDialog);
    }
}

void CloseLoadingProgressDialog() {
    if (g_progressDialog) {
        DestroyWindow(g_progressDialog);
        g_progressDialog = nullptr;
    }
    EnableWindow(g_mainWindow, TRUE);
    SetActiveWindow(g_mainWindow);
}

// Async loading with progress dialog
void LoadHaldCLUTDatabaseAsync() {
    if (g_haldCLUTBasePath.empty()) {
        // No path configured, show dialog
        int result = MessageBoxW(g_mainWindow, 
            L"HaldCLUT directory not configured.\n\nWould you like to select a directory now?",
            L"HaldCLUT Configuration", MB_YESNO | MB_ICONQUESTION);
        if (result == IDYES) {
            ShowConfigDialog(g_mainWindow);
        }
        if (g_haldCLUTBasePath.empty()) {
            g_haldCLUTPanelVisible = false;
            return;
        }
    }
    
    if (GetFileAttributesW(g_haldCLUTBasePath.c_str()) == INVALID_FILE_ATTRIBUTES) {
        ShowError(L"HaldCLUT directory does not exist. Please configure a valid path in Tools menu.");
        g_haldCLUTBasePath.clear();
        SaveConfig();
        g_haldCLUTPanelVisible = false;
        return;
    }

    // Clear old data
    g_haldCLUTCategories.clear();
    g_selectedHaldCLUT.Reset();
    if (g_haldCLUTImage) {
        delete g_haldCLUTImage;
        g_haldCLUTImage = nullptr;
        g_haldCLUTBitmap = nullptr;
    }

    // Create progress tracking
    LoadingProgress progress;
    
    // Show progress dialog
    ShowLoadingProgressDialog(&progress);
    
    // Load in background thread
    std::thread loadThread([&progress]() {
        LoadHaldCLUTDirectoryRecursive(g_haldCLUTBasePath, L"HaldCLUT", &progress);
        progress.completed.store(true);
    });
    
    // Process messages while loading
    MSG msg;
    while (!progress.completed.load() && !progress.cancelled.load()) {
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                progress.cancelled.store(true);
                // Re-post WM_QUIT so the outer message loop sees it
                PostQuitMessage(static_cast<int>(msg.wParam));
            } else if (!IsDialogMessageW(g_progressDialog, &msg)) {
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
        }
        Sleep(10);
    }
    
    loadThread.join();
    CloseLoadingProgressDialog();
    
    if (progress.cancelled.load()) {
        // User cancelled, clear partial data
        g_haldCLUTCategories.clear();
        g_haldCLUTPanelVisible = false;
    } else {
        UpdateHaldCLUTLayout();
        InvalidateRect(g_haldCLUTPanel, nullptr, FALSE);
        
        // Update status
        int totalCLUTs = 0;
        for (const auto& cat : g_haldCLUTCategories) {
            totalCLUTs += static_cast<int>(cat.entries.size());
        }
        std::wstring status = L"Loaded " + std::to_wstring(totalCLUTs) + L" HaldCLUTs in " + 
                              std::to_wstring(g_haldCLUTCategories.size()) + L" categories";
        SetWindowTextW(g_statusBar, status.c_str());
    }
}

void UpdateHaldCLUTLayout() {
    if (!g_haldCLUTPanel) return;

    RECT rc{};
    GetClientRect(g_haldCLUTPanel, &rc);
    const int pageHeight = std::max<int>(0, rc.bottom - rc.top);

    g_haldCLUTTotalHeight = 0;
    // Header height
    g_haldCLUTTotalHeight += 30;
    // Original entry
    g_haldCLUTTotalHeight += 24;

    for (const auto& cat : g_haldCLUTCategories) {
        g_haldCLUTTotalHeight += 26;  // Category header
        if (cat.expanded) {
            g_haldCLUTTotalHeight += static_cast<int>(cat.entries.size()) * 24;  // Entries
        }
    }

    SCROLLINFO si{};
    si.cbSize = sizeof(si);
    si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
    si.nMin = 0;
    si.nMax = std::max(0, g_haldCLUTTotalHeight - 1);
    si.nPage = static_cast<UINT>(pageHeight);
    si.nPos = std::clamp(g_haldCLUTScrollPos, 0, std::max(0, g_haldCLUTTotalHeight - pageHeight));
    g_haldCLUTScrollPos = si.nPos;
    SetScrollInfo(g_haldCLUTPanel, SB_VERT, &si, TRUE);
}

void ScrollHaldCLUT(int delta) {
    if (!g_haldCLUTPanel) return;

    RECT rc{};
    GetClientRect(g_haldCLUTPanel, &rc);
    const int pageHeight = std::max<int>(0, rc.bottom - rc.top);
    const int maxPos = std::max<int>(0, g_haldCLUTTotalHeight - pageHeight);
    g_haldCLUTScrollPos = std::clamp(g_haldCLUTScrollPos + delta, 0, maxPos);

    SCROLLINFO si{};
    si.cbSize = sizeof(si);
    si.fMask = SIF_POS;
    si.nPos = g_haldCLUTScrollPos;
    SetScrollInfo(g_haldCLUTPanel, SB_VERT, &si, TRUE);
    InvalidateRect(g_haldCLUTPanel, nullptr, FALSE);
}

void ApplyHaldCLUTToBitmap(Bitmap* source, Bitmap* clut, int level) {
    if (g_haldCLUTApplyMode == HaldCLUTApplyMode::SmoothInterpolated) {
        ApplyHaldCLUTToBitmapSmooth(source, clut, level);
    } else {
        ApplyHaldCLUTToBitmapMx(source, clut);
    }
}

void ApplyHaldCLUTToBitmapMx(Bitmap* source, Bitmap* clut) {
    if (!source || !clut) return;

    int srcW = static_cast<int>(source->GetWidth());
    int srcH = static_cast<int>(source->GetHeight());
    int clutW = static_cast<int>(clut->GetWidth());
    int clutH = static_cast<int>(clut->GetHeight());
    if (clutW <= 0 || clutH <= 0 || clutW != clutH) {
        return;
    }

    const int haldOrder = static_cast<int>(std::lround(std::pow(static_cast<double>(clutW), 1.0 / 3.0)));
    const int cubeSize = haldOrder * haldOrder;
    if (haldOrder <= 0 || cubeSize <= 1 || cubeSize * cubeSize * cubeSize != clutW * clutH) {
        return;
    }

    Rect srcRect(0, 0, srcW, srcH);
    BitmapData srcData;
    if (source->LockBits(&srcRect, ImageLockModeRead | ImageLockModeWrite,
                         PixelFormat32bppARGB, &srcData) != Ok) {
        return;
    }

    Rect clutRect(0, 0, clutW, clutH);
    BitmapData clutData;
    if (clut->LockBits(&clutRect, ImageLockModeRead,
                       PixelFormat32bppARGB, &clutData) != Ok) {
        source->UnlockBits(&srcData);
        return;
    }

    BYTE* srcPixels = static_cast<BYTE*>(srcData.Scan0);
    BYTE* clutPixels = static_cast<BYTE*>(clutData.Scan0);
    int srcStride = srcData.Stride;
    int clutStride = clutData.Stride;

    float scale = (cubeSize - 1) / 255.0f;
    std::vector<int> channelLookup(256);
    for (int i = 0; i < 256; ++i) {
        channelLookup[i] = std::clamp(static_cast<int>(i * scale + 0.5f), 0, cubeSize - 1);
    }

    for (int y = 0; y < srcH; ++y) {
        for (int x = 0; x < srcW; ++x) {
            BYTE* pixel = srcPixels + y * srcStride + x * 4;
            BYTE b = pixel[0];
            BYTE g = pixel[1];
            BYTE r = pixel[2];

            const int clutR = channelLookup[r];
            const int clutG = channelLookup[g];
            const int clutB = channelLookup[b];
            const int linearIndex = clutR + cubeSize * clutG + cubeSize * cubeSize * clutB;
            const int clutX = linearIndex % clutW;
            const int clutY = linearIndex / clutW;

            if (clutX >= 0 && clutX < clutW && clutY >= 0 && clutY < clutH) {
                BYTE* clutPixel = clutPixels + clutY * clutStride + clutX * 4;
                pixel[0] = clutPixel[0];
                pixel[1] = clutPixel[1];
                pixel[2] = clutPixel[2];
            }
        }
    }

    clut->UnlockBits(&clutData);
    source->UnlockBits(&srcData);
}

void ApplyHaldCLUTToBitmapSmooth(Bitmap* source, Bitmap* clut, int level) {
    if (!source || !clut || level <= 0) return;

    int srcW = static_cast<int>(source->GetWidth());
    int srcH = static_cast<int>(source->GetHeight());
    int clutW = static_cast<int>(clut->GetWidth());
    int clutH = static_cast<int>(clut->GetHeight());
    if (clutW <= 0 || clutH <= 0 || clutW != clutH) {
        return;
    }

    const int cubeSize = level;
    if (cubeSize <= 1 || cubeSize * cubeSize * cubeSize != clutW * clutH) {
        return;
    }

    Rect srcRect(0, 0, srcW, srcH);
    BitmapData srcData;
    if (source->LockBits(&srcRect, ImageLockModeRead | ImageLockModeWrite,
                         PixelFormat32bppARGB, &srcData) != Ok) {
        return;
    }

    Rect clutRect(0, 0, clutW, clutH);
    BitmapData clutData;
    if (clut->LockBits(&clutRect, ImageLockModeRead,
                       PixelFormat32bppARGB, &clutData) != Ok) {
        source->UnlockBits(&srcData);
        return;
    }

    BYTE* srcPixels = static_cast<BYTE*>(srcData.Scan0);
    BYTE* clutPixels = static_cast<BYTE*>(clutData.Scan0);
    int srcStride = srcData.Stride;
    int clutStride = clutData.Stride;

    float scale = (cubeSize - 1) / 255.0f;

    struct ChannelLookup {
        int low;
        int high;
        float fraction;
    };

    std::vector<ChannelLookup> channelLookup(256);
    for (int i = 0; i < 256; ++i) {
        float scaled = i * scale;
        int low = static_cast<int>(scaled);
        int high = std::min(low + 1, cubeSize - 1);
        channelLookup[i] = {low, high, scaled - low};
    }

    auto ReadClutColor = [&](int rIndex, int gIndex, int bIndex, int channel) -> float {
        const int linearIndex = bIndex * cubeSize * cubeSize + gIndex * cubeSize + rIndex;
        const int clutX = linearIndex % clutW;
        const int clutY = linearIndex / clutW;
        BYTE* clutPixel = clutPixels + clutY * clutStride + clutX * 4;
        return static_cast<float>(clutPixel[channel]);
    };

    auto Lerp = [](float a, float b, float t) -> float {
        return a + (b - a) * t;
    };

    auto SampleClutChannel = [&](const ChannelLookup& rInfo, const ChannelLookup& gInfo,
                                 const ChannelLookup& bInfo, int channel) -> BYTE {
        float c000 = ReadClutColor(rInfo.low,  gInfo.low,  bInfo.low,  channel);
        float c100 = ReadClutColor(rInfo.high, gInfo.low,  bInfo.low,  channel);
        float c010 = ReadClutColor(rInfo.low,  gInfo.high, bInfo.low,  channel);
        float c110 = ReadClutColor(rInfo.high, gInfo.high, bInfo.low,  channel);
        float c001 = ReadClutColor(rInfo.low,  gInfo.low,  bInfo.high, channel);
        float c101 = ReadClutColor(rInfo.high, gInfo.low,  bInfo.high, channel);
        float c011 = ReadClutColor(rInfo.low,  gInfo.high, bInfo.high, channel);
        float c111 = ReadClutColor(rInfo.high, gInfo.high, bInfo.high, channel);

        float c00 = Lerp(c000, c100, rInfo.fraction);
        float c10 = Lerp(c010, c110, rInfo.fraction);
        float c01 = Lerp(c001, c101, rInfo.fraction);
        float c11 = Lerp(c011, c111, rInfo.fraction);
        float c0 = Lerp(c00, c10, gInfo.fraction);
        float c1 = Lerp(c01, c11, gInfo.fraction);
        float value = Lerp(c0, c1, bInfo.fraction);
        return static_cast<BYTE>(std::clamp(value, 0.0f, 255.0f) + 0.5f);
    };

    for (int y = 0; y < srcH; ++y) {
        for (int x = 0; x < srcW; ++x) {
            BYTE* pixel = srcPixels + y * srcStride + x * 4;
            const ChannelLookup& rInfo = channelLookup[pixel[2]];
            const ChannelLookup& gInfo = channelLookup[pixel[1]];
            const ChannelLookup& bInfo = channelLookup[pixel[0]];

            pixel[0] = SampleClutChannel(rInfo, gInfo, bInfo, 0);
            pixel[1] = SampleClutChannel(rInfo, gInfo, bInfo, 1);
            pixel[2] = SampleClutChannel(rInfo, gInfo, bInfo, 2);
        }
    }

    clut->UnlockBits(&clutData);
    source->UnlockBits(&srcData);
}

void ClearLoadedHaldCLUT() {
    if (g_haldCLUTImage) {
        delete g_haldCLUTImage;
        g_haldCLUTImage = nullptr;
    }
    g_haldCLUTBitmap = nullptr;
}

void LoadSelectedHaldCLUT() {
    if (!g_selectedHaldCLUT.IsValid()) {
        ClearLoadedHaldCLUT();
        return;
    }

    HaldCLUTCategory& cat = g_haldCLUTCategories[g_selectedHaldCLUT.categoryIndex];
    HaldCLUTEntry& entry = cat.entries[g_selectedHaldCLUT.entryIndex];

    ClearLoadedHaldCLUT();

    std::unique_ptr<Image> loadedImage(Image::FromFile(entry.path.c_str(), FALSE));
    if (!loadedImage || loadedImage->GetLastStatus() != Ok) {
        return;
    }

    Bitmap* normalizedBitmap = new Bitmap(entry.width, entry.height, PixelFormat32bppARGB);
    if (!normalizedBitmap || normalizedBitmap->GetLastStatus() != Ok) {
        delete normalizedBitmap;
        return;
    }

    Graphics graphics(normalizedBitmap);
    if (graphics.GetLastStatus() != Ok) {
        delete normalizedBitmap;
        return;
    }

    graphics.DrawImage(loadedImage.get(), 0, 0, entry.width, entry.height);
    if (graphics.GetLastStatus() != Ok) {
        delete normalizedBitmap;
        return;
    }

    g_haldCLUTImage = normalizedBitmap;
    g_haldCLUTBitmap = normalizedBitmap;
}

void ToggleHaldCLUTPanel() {
    g_haldCLUTPanelVisible = !g_haldCLUTPanelVisible;

    if (g_haldCLUTPanelVisible && g_haldCLUTCategories.empty()) {
        LoadHaldCLUTDatabaseAsync();
    }

    if (g_haldCLUTPanel) {
        ShowWindow(g_haldCLUTPanel, g_haldCLUTPanelVisible ? SW_SHOW : SW_HIDE);
    }

    LayoutChildren();
    InvalidateRect(g_mainWindow, nullptr, FALSE);
}

LRESULT CALLBACK HaldCLUTViewProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        return 0;

    case WM_SIZE:
        UpdateHaldCLUTLayout();
        return 0;

    case WM_MOUSEWHEEL:
        ScrollHaldCLUT(-(GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA) * 48);
        return 0;

    case WM_VSCROLL: {
        SCROLLINFO si{};
        si.cbSize = sizeof(si);
        si.fMask = SIF_ALL;
        GetScrollInfo(hwnd, SB_VERT, &si);

        int newPos = si.nPos;
        switch (LOWORD(wParam)) {
        case SB_LINEUP:
            newPos -= 48;
            break;
        case SB_LINEDOWN:
            newPos += 48;
            break;
        case SB_PAGEUP:
            newPos -= static_cast<int>(si.nPage);
            break;
        case SB_PAGEDOWN:
            newPos += static_cast<int>(si.nPage);
            break;
        case SB_THUMBTRACK:
        case SB_THUMBPOSITION:
            newPos = si.nTrackPos;
            break;
        }

        g_haldCLUTScrollPos = std::clamp(newPos, si.nMin,
                                         std::max(si.nMin, si.nMax - static_cast<int>(si.nPage) + 1));
        si.fMask = SIF_POS;
        si.nPos = g_haldCLUTScrollPos;
        SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }

    case WM_MOUSEMOVE: {
        POINT point = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        int y = point.y + g_haldCLUTScrollPos - 30;  // Offset for header

        int hoverIndex = -1;
        bool hoverOriginal = false;
        int currentY = 0;

        if (y >= currentY && y < currentY + 24) {
            hoverOriginal = true;
        }
        currentY += 24;

        for (size_t i = 0; i < g_haldCLUTCategories.size(); ++i) {
            if (y >= currentY && y < currentY + 26) {
                hoverIndex = static_cast<int>(i);
                break;
            }
            currentY += 26;

            if (g_haldCLUTCategories[i].expanded) {
                int entryCount = static_cast<int>(g_haldCLUTCategories[i].entries.size());
                if (y >= currentY && y < currentY + entryCount * 24) {
                    hoverIndex = static_cast<int>(i);
                    break;
                }
                currentY += entryCount * 24;
            }
        }

        if (hoverIndex != g_haldCLUTHoverIndex || hoverOriginal != g_haldCLUTHoverOriginal) {
            g_haldCLUTHoverIndex = hoverIndex;
            g_haldCLUTHoverOriginal = hoverOriginal;
            InvalidateRect(hwnd, nullptr, FALSE);
        }

        if (g_haldCLUTPanel) {
            TRACKMOUSEEVENT tme{sizeof(tme), TME_LEAVE, hwnd, 0};
            TrackMouseEvent(&tme);
        }
        return 0;
    }

    case WM_MOUSELEAVE:
        g_haldCLUTHoverIndex = -1;
        g_haldCLUTHoverOriginal = false;
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;

    case WM_LBUTTONUP: {
        POINT point = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        RECT client{};
        GetClientRect(hwnd, &client);

        if (point.y >= 0 && point.y < 30 && point.x >= client.right - 100 && point.x < client.right) {
            ShowConfigDialog(g_mainWindow ? g_mainWindow : hwnd);
            return 0;
        }

        int y = point.y + g_haldCLUTScrollPos - 30;

        int currentY = 0;
        if (y >= currentY && y < currentY + 24) {
            g_selectedHaldCLUT.Reset();
            ClearLoadedHaldCLUT();
            UpdateStatusBar();
            
            // Force immediate repaint of the panel to show selection change
            InvalidateRect(hwnd, nullptr, FALSE);
            UpdateWindow(hwnd);  // Synchronously process WM_PAINT immediately
            
            if (g_mainWindow) {
                SetFocus(g_mainWindow);
                // Post message to update main window image after UI is refreshed
                PostMessage(g_mainWindow, kLoadHaldCLUTMsg, 0, 0);
            }
            return 0;
        }
        currentY += 24;

        for (size_t i = 0; i < g_haldCLUTCategories.size(); ++i) {
            // Check category header click
            if (y >= currentY && y < currentY + 26) {
                g_haldCLUTCategories[i].expanded = !g_haldCLUTCategories[i].expanded;
                UpdateHaldCLUTLayout();
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
            currentY += 26;

            // Check entry clicks
            if (g_haldCLUTCategories[i].expanded) {
                int entryCount = static_cast<int>(g_haldCLUTCategories[i].entries.size());
                for (int j = 0; j < entryCount; ++j) {
                    if (y >= currentY && y < currentY + 24) {
                        // Select this entry - use indices instead of pointer
                        g_selectedHaldCLUT.categoryIndex = static_cast<int>(i);
                        g_selectedHaldCLUT.entryIndex = j;
                        
                        // Immediately update status bar
                        UpdateStatusBar();
                        
                        // Force immediate repaint of the panel to show selection BEFORE any blocking operation
                        // Invalidate the entire panel to ensure proper redraw
                        InvalidateRect(hwnd, nullptr, FALSE);
                        UpdateWindow(hwnd);  // Synchronously process WM_PAINT immediately
                        
                        // Set focus to main window and defer LUT loading
                        // Use PostMessage instead of SetTimer for more predictable timing
                        if (g_mainWindow) {
                            SetFocus(g_mainWindow);
                            // Post the load message to the back of the queue, allowing paint messages to process first
                            PostMessage(g_mainWindow, kLoadHaldCLUTMsg, 0, 0);
                        }
                        return 0;
                    }
                    currentY += 24;
                }
            }
        }
        return 0;
    }

    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT: {
        PAINTSTRUCT ps{};
        HDC dc = BeginPaint(hwnd, &ps);

        RECT client{};
        GetClientRect(hwnd, &client);
        int width = client.right - client.left;
        int height = client.bottom - client.top;
        
        // Double buffering to prevent flickering during scroll
        HDC memoryDc = CreateCompatibleDC(dc);
        HBITMAP backBuffer = CreateCompatibleBitmap(dc, width, height);
        HBITMAP oldBitmap = static_cast<HBITMAP>(SelectObject(memoryDc, backBuffer));

        FillRect(memoryDc, &client, g_brushPanel);

        SetBkMode(memoryDc, TRANSPARENT);
        HFONT oldFont = static_cast<HFONT>(SelectObject(memoryDc, g_uiFont));

        int y = -g_haldCLUTScrollPos;

        // Draw header
        RECT headerRect = {0, y, width, y + 30};
        FillRect(memoryDc, &headerRect, g_brushHeader);
        SetTextColor(memoryDc, kColorTextPrimary);
        SelectObject(memoryDc, g_uiFontBold);
        RECT textRect = {10, y, width - 10, y + 30};
        DrawTextW(memoryDc, L"HaldCLUT Filters", -1, &textRect, DT_VCENTER | DT_SINGLELINE);
        
        // Draw config button hint
        SetTextColor(memoryDc, kColorTextSecondary);
        RECT hintRect = {width - 100, y, width - 10, y + 30};
        DrawTextW(memoryDc, L"[Configure]", -1, &hintRect, DT_VCENTER | DT_RIGHT | DT_SINGLELINE);
        
        y += 30;

        SelectObject(memoryDc, g_uiFont);

        RECT originalRect = {20, y, width, y + 24};
        bool isOriginalSelected = !g_selectedHaldCLUT.IsValid();
        if (isOriginalSelected) {
            HBRUSH selBrush = CreateSolidBrush(kColorAccent);
            FillRect(memoryDc, &originalRect, selBrush);
            DeleteObject(selBrush);
            SetTextColor(memoryDc, kColorWindowBg);
        } else if (g_haldCLUTHoverOriginal) {
            FillRect(memoryDc, &originalRect, g_brushHeaderHover);
            SetTextColor(memoryDc, kColorTextPrimary);
        } else {
            SetTextColor(memoryDc, kColorTextPrimary);
        }
        RECT originalTextRect = {40, y, width - 10, y + 24};
        DrawTextW(memoryDc, L"Original", -1, &originalTextRect, DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        y += 24;

        // Draw categories and entries
        for (size_t i = 0; i < g_haldCLUTCategories.size(); ++i) {
            const auto& cat = g_haldCLUTCategories[i];

            // Category header
            RECT catRect = {0, y, width, y + 26};
            if (static_cast<int>(i) == g_haldCLUTHoverIndex) {
                FillRect(memoryDc, &catRect, g_brushHeaderHover);
            }

            // Expand/collapse indicator
            SetTextColor(memoryDc, kColorTextSecondary);
            RECT indicatorRect = {10, y, 30, y + 26};
            DrawTextW(memoryDc, cat.expanded ? L"-" : L"+", -1, &indicatorRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

            // Category name
            RECT nameRect = {30, y, width - 10, y + 26};
            DrawTextW(memoryDc, cat.name.c_str(), -1, &nameRect, DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            y += 26;

            // Entries
            if (cat.expanded) {
                for (size_t j = 0; j < cat.entries.size(); ++j) {
                    RECT entryRect = {20, y, width, y + 24};

                    // Check if this entry is selected
                    bool isSelected = (g_selectedHaldCLUT.categoryIndex == static_cast<int>(i) && 
                                       g_selectedHaldCLUT.entryIndex == static_cast<int>(j));
                    if (isSelected) {
                        HBRUSH selBrush = CreateSolidBrush(kColorAccent);
                        FillRect(memoryDc, &entryRect, selBrush);
                        DeleteObject(selBrush);
                        SetTextColor(memoryDc, kColorWindowBg);
                    } else {
                        SetTextColor(memoryDc, kColorTextPrimary);
                    }

                    RECT entryTextRect = {40, y, width - 10, y + 24};
                    DrawTextW(memoryDc, cat.entries[j].name.c_str(), -1, &entryTextRect,
                              DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
                    y += 24;
                }
            }
        }

        // Border
        HPEN borderPen = CreatePen(PS_SOLID, 1, kColorBorder);
        HPEN oldPen = static_cast<HPEN>(SelectObject(memoryDc, borderPen));
        HGDIOBJ oldBrush = SelectObject(memoryDc, GetStockObject(NULL_BRUSH));
        Rectangle(memoryDc, client.left, client.top, client.right, client.bottom);
        SelectObject(memoryDc, oldBrush);
        SelectObject(memoryDc, oldPen);
        DeleteObject(borderPen);

        SelectObject(memoryDc, oldFont);
        
        // Copy to screen
        BitBlt(dc, 0, 0, width, height, memoryDc, 0, 0, SRCCOPY);
        
        SelectObject(memoryDc, oldBitmap);
        DeleteObject(backBuffer);
        DeleteDC(memoryDc);
        
        EndPaint(hwnd, &ps);
        return 0;
    }
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        g_mainWindow = hwnd;

        NONCLIENTMETRICSW metrics{};
        metrics.cbSize = sizeof(metrics);
        SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0);
        g_uiFont = CreateFontIndirectW(&metrics.lfMessageFont);
        LOGFONTW bold = metrics.lfMessageFont;
        bold.lfWeight = FW_SEMIBOLD;
        g_uiFontBold = CreateFontIndirectW(&bold);
        g_brushWindow = CreateSolidBrush(kColorWindowBg);
        g_brushPanel = CreateSolidBrush(kColorPanelBg);
        g_brushHeader = CreateSolidBrush(kColorPanelHeader);
        g_brushHeaderHover = CreateSolidBrush(kColorPanelHeaderHover);
        g_brushStatus = CreateSolidBrush(kColorPanelBg);

        CreateMainMenu(hwnd);

        g_metadataHeader = CreateWindowW(
            L"BUTTON", L"Info",
            WS_CHILD | BS_OWNERDRAW,
            0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(1001), nullptr, nullptr);
        SendMessageW(g_metadataHeader, WM_SETFONT, reinterpret_cast<WPARAM>(g_uiFont), TRUE);

        g_metadataView = CreateWindowExW(
            0, kMetadataViewClassName, L"",
            WS_CHILD | WS_VSCROLL,
            0, 0, 0, 0, hwnd, nullptr, nullptr, nullptr);

        g_haldCLUTPanel = CreateWindowExW(
            0, kHaldCLUTViewClassName, L"",
            WS_CHILD | WS_VSCROLL,
            0, 0, 0, 0, hwnd, nullptr, nullptr, nullptr);

        g_statusBar = CreateWindowW(
            L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_LEFTNOWORDWRAP,
            0, 0, 0, 0, hwnd, nullptr, nullptr, nullptr);
        SendMessageW(g_statusBar, WM_SETFONT, reinterpret_cast<WPARAM>(g_uiFont), TRUE);

        DragAcceptFiles(hwnd, TRUE);
        UpdateStatusBar();
        RefreshMetadataPanel();
        LayoutChildren();
        return 0;
    }

    case WM_COMMAND:
        if (LOWORD(wParam) == 1001 && HIWORD(wParam) == BN_CLICKED) {
            if (g_panelVisible) {
                g_panelCollapsed = !g_panelCollapsed;
                RefreshMetadataPanel();
                LayoutChildren();
                FitToWindow();
                InvalidateRect(hwnd, nullptr, FALSE);
                SetFocus(hwnd);
            }
            return 0;
        }
        
        // Menu commands
        switch (LOWORD(wParam)) {
        case kMenuIdFileOpen:
            OpenImageDialog(hwnd);
            return 0;
        case kMenuIdFileSavePreviewAs:
            SaveCurrentPreviewAs(hwnd);
            return 0;
        case kMenuIdViewHaldCLUT:
            ToggleHaldCLUTPanel();
            return 0;
        case kMenuIdViewHaldCLUTModeMx:
            g_haldCLUTApplyMode = HaldCLUTApplyMode::MxCompatible;
            UpdateMenuState();
            UpdateStatusBar();
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        case kMenuIdViewHaldCLUTModeSmooth:
            g_haldCLUTApplyMode = HaldCLUTApplyMode::SmoothInterpolated;
            UpdateMenuState();
            UpdateStatusBar();
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        case kMenuIdToolsAssociate:
            ShowAssociationDialog(hwnd);
            return 0;
        case kMenuIdHelpVisitGitHub:
            ShellExecuteW(hwnd, L"open", L"https://github.com/aiimagestudio/NoctisViewer", nullptr, nullptr, SW_SHOWNORMAL);
            return 0;
        case kMenuIdHelpAbout: {
            std::wstring aboutText = std::wstring(L"Noctis Viewer\n\nVersion ") + kAppVersion +
                L"\n\nA fast, minimal native image viewer for Windows.\n"
                L"Supports PNG, JPG, BMP, GIF, and TIFF formats.\n\n"
                L"Keyboard shortcuts:\n"
                L"• Arrow keys / Mouse wheel - Navigate images\n"
                L"• Home / End - Jump to first / last image\n"
                L"• Page Up / Page Down - Zoom in / out\n"
                L"• Ctrl + Mouse wheel - Zoom in / out\n"
                L"• Delete - Delete current image\n"
                L"• H - Toggle HaldCLUT panel\n"
                L"• Space (hold) - Preview original image\n"
                L"• File > Save Current Preview As... - Export the current LUT preview\n"
                L"• Double-click - Open file dialog\n\n"
                L"Visit GitHub: https://github.com/aiimagestudio/NoctisViewer";
            MessageBoxW(hwnd, aboutText.c_str(), L"About Noctis Viewer", MB_OK | MB_ICONINFORMATION);
            return 0;
        }
        }
        break;

    case WM_CTLCOLORSTATIC: {
        HDC dc = reinterpret_cast<HDC>(wParam);
        HWND control = reinterpret_cast<HWND>(lParam);
        if (control == g_statusBar) {
            SetTextColor(dc, kColorTextSecondary);
            SetBkColor(dc, kColorPanelBg);
            SetBkMode(dc, OPAQUE);
            return reinterpret_cast<INT_PTR>(g_brushStatus);
        }
        break;
    }

    case WM_DRAWITEM: {
        const DRAWITEMSTRUCT* dis = reinterpret_cast<const DRAWITEMSTRUCT*>(lParam);
        if (dis && dis->CtlID == 1001) {
            HBRUSH brush = g_headerHot ? g_brushHeaderHover : g_brushHeader;
            FillRect(dis->hDC, &dis->rcItem, brush);
            FrameRect(dis->hDC, &dis->rcItem, g_brushWindow);

            SetBkMode(dis->hDC, TRANSPARENT);
            SetTextColor(dis->hDC, kColorTextPrimary);
            HFONT oldFont = static_cast<HFONT>(SelectObject(dis->hDC, g_uiFontBold));
            wchar_t text[128] = {};
            GetWindowTextW(dis->hwndItem, text, 128);
            RECT textRect = dis->rcItem;
            DrawTextW(dis->hDC, text, -1, &textRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            SelectObject(dis->hDC, oldFont);

            if (dis->itemState & ODS_FOCUS) {
                RECT focusRect = dis->rcItem;
                InflateRect(&focusRect, -3, -3);
                DrawFocusRect(dis->hDC, &focusRect);
            }
            return TRUE;
        }
        break;
    }

    case WM_TIMER:
        if (wParam == kLoadHaldCLUTMsg) {
            KillTimer(hwnd, kLoadHaldCLUTMsg);
            // FALL THROUGH to custom message handler for LUT loading
        }
        return 0;
    
    case kLoadHaldCLUTMsg:
        // Load LUT file - this is posted via PostMessage to ensure UI updates first
        LoadSelectedHaldCLUT();
        UpdateStatusBar();
        // Invalidate the main image viewport to show the LUT effect
        if (g_currentImage) {
            RECT viewport = GetImageViewportRect();
            InvalidateRect(hwnd, &viewport, FALSE);
        }
        return 0;

    case WM_GETMINMAXINFO: {
        auto* info = reinterpret_cast<MINMAXINFO*>(lParam);
        float dpiScale = GetDpiScale();
        info->ptMinTrackSize.x = static_cast<int>(kMinWindowWidth * dpiScale);
        info->ptMinTrackSize.y = static_cast<int>(kMinWindowHeight * dpiScale);
        return 0;
    }

    case WM_MOUSEMOVE: {
        POINT point = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        
        // Handle image panning with cached backbuffer for smooth dragging
        if (g_isDragging && g_currentImage) {
            int deltaX = point.x - g_dragStartPos.x;
            int deltaY = point.y - g_dragStartPos.y;
            g_panOffsetX = g_dragStartPanX + deltaX;
            g_panOffsetY = g_dragStartPanY + deltaY;
            
            // Direct paint using cached backbuffer - only paint viewport area
            HDC dc = GetDC(hwnd);
            if (dc && g_cachedMemoryDc) {
                RECT viewport = GetImageViewportRect();
                int viewWidth = std::max<int>(1, viewport.right - viewport.left);
                int viewHeight = std::max<int>(1, viewport.bottom - viewport.top);
                
                RECT bufRect = {0, 0, viewWidth, viewHeight};
                FillRect(g_cachedMemoryDc, &bufRect, g_brushWindow);
                PaintImage(g_cachedMemoryDc);
                BitBlt(dc, 0, 0, viewWidth, viewHeight, g_cachedMemoryDc, 0, 0, SRCCOPY);
                
                ReleaseDC(hwnd, dc);
            }
        }
        
        if (g_metadataHeader) {
            RECT rc{};
            GetWindowRect(g_metadataHeader, &rc);
            MapWindowPoints(nullptr, hwnd, reinterpret_cast<LPPOINT>(&rc), 2);
            const bool hot = PtInRect(&rc, point);
            if (hot != g_headerHot) {
                g_headerHot = hot;
                InvalidateRect(g_metadataHeader, nullptr, FALSE);
            }
        }
        TRACKMOUSEEVENT tme{sizeof(tme), TME_LEAVE, hwnd, 0};
        TrackMouseEvent(&tme);
        break;
    }

    case WM_MOUSELEAVE:
        if (g_headerHot) {
            g_headerHot = false;
            InvalidateRect(g_metadataHeader, nullptr, FALSE);
        }
        return 0;

    case WM_DROPFILES: {
        HDROP drop = reinterpret_cast<HDROP>(wParam);
        wchar_t filePath[MAX_PATH] = L"";
        if (DragQueryFileW(drop, 0, filePath, MAX_PATH) > 0) {
            if (IsSupportedImageFile(filePath)) {
                LoadImageFile(filePath);
            } else {
                ShowError(L"Only common image formats are supported.");
            }
        }
        DragFinish(drop);
        return 0;
    }

    case WM_MOUSEWHEEL:
        // Check if over metadata view
        if (g_panelVisible && !g_panelCollapsed && g_metadataView) {
            POINT point = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            RECT rc{};
            GetWindowRect(g_metadataView, &rc);
            if (PtInRect(&rc, point)) {
                return SendMessageW(g_metadataView, msg, wParam, lParam);
            }
        }
        // Check if over HaldCLUT panel BEFORE navigating images
        if (g_haldCLUTPanelVisible && g_haldCLUTPanel) {
            POINT point = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            RECT rc{};
            GetWindowRect(g_haldCLUTPanel, &rc);
            if (PtInRect(&rc, point)) {
                return SendMessageW(g_haldCLUTPanel, msg, wParam, lParam);
            }
        }
        // Check for Ctrl+Wheel zoom
        if (GetKeyState(VK_CONTROL) & 0x8000) {
            if (GET_WHEEL_DELTA_WPARAM(wParam) > 0) {
                ZoomIn();
            } else {
                ZoomOut();
            }
            return 0;
        }
        // Navigate images
        if (GET_WHEEL_DELTA_WPARAM(wParam) > 0) {
            NavigateRelative(-1);
        } else {
            NavigateRelative(1);
        }
        return 0;

    case WM_KEYDOWN:
        // Check for Ctrl+O (Open)
        if ((GetKeyState(VK_CONTROL) & 0x8000) && (wParam == 'O' || wParam == 'o')) {
            OpenImageDialog(hwnd);
            return 0;
        }
        switch (wParam) {
        case VK_LEFT:
        case VK_UP:
            NavigateRelative(-1);
            return 0;
        case VK_RIGHT:
        case VK_DOWN:
            NavigateRelative(1);
            return 0;
        case VK_PRIOR:
        case VK_ADD:
            ZoomIn();
            return 0;
        case VK_NEXT:
        case VK_SUBTRACT:
            ZoomOut();
            return 0;
        case VK_DELETE:
            DeleteCurrentImage();
            return 0;
        case VK_HOME:
            NavigateToFirst();
            return 0;
        case VK_END:
            NavigateToLast();
            return 0;
        case 'H':
        case 'h':
            ToggleHaldCLUTPanel();
            return 0;
        case VK_SPACE:
            if (!g_showingOriginal && g_selectedHaldCLUT.IsValid() && g_currentImage) {
                g_showingOriginal = true;
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        default:
            break;
        }
        break;

    case WM_KEYUP:
        if (wParam == VK_SPACE && g_showingOriginal) {
            g_showingOriginal = false;
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        break;

    case WM_LBUTTONDOWN: {
        const int x = GET_X_LPARAM(lParam);
        const int y = GET_Y_LPARAM(lParam);
        RECT viewport = GetImageViewportRect();
        if (g_currentImage && PtInRect(&viewport, POINT{x, y})) {
            g_isDragging = true;
            g_dragStartPos = {x, y};
            g_dragStartPanX = g_panOffsetX;
            g_dragStartPanY = g_panOffsetY;
            SetCapture(hwnd);
        }
        break;
    }

    case WM_LBUTTONUP:
        if (g_isDragging) {
            g_isDragging = false;
            ReleaseCapture();
        }
        break;

    case WM_LBUTTONDBLCLK: {
        const int x = GET_X_LPARAM(lParam);
        const int y = GET_Y_LPARAM(lParam);
        RECT viewport = GetImageViewportRect();
        if (PtInRect(&viewport, POINT{x, y})) {
            OpenImageDialog(hwnd);
            return 0;
        }
        break;
    }
    
    case WM_SIZE:
        // Clear cached buffer on resize - it will be recreated with new size when needed
        ClearBackbufferCache();
        LayoutChildren(false);
        FitToWindow();
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;

    case WM_PAINT: {
        PAINTSTRUCT ps{};
        HDC dc = BeginPaint(hwnd, &ps);
        
        // Only paint the image viewport area, not the entire client area
        // The panels are child windows and handle their own painting
        RECT viewport = GetImageViewportRect();
        int viewWidth = std::max<int>(1, viewport.right - viewport.left);
        int viewHeight = std::max<int>(1, viewport.bottom - viewport.top);
        
        // Use cached backbuffer sized to viewport for smooth rendering
        EnsureBackbuffer(viewWidth, viewHeight);
        if (g_cachedMemoryDc) {
            // Fill and paint only the viewport area
            RECT bufRect = {0, 0, viewWidth, viewHeight};
            FillRect(g_cachedMemoryDc, &bufRect, g_brushWindow);
            PaintImage(g_cachedMemoryDc);
            
            // Copy only the viewport area to screen
            BitBlt(dc, 0, 0, viewWidth, viewHeight, g_cachedMemoryDc, 0, 0, SRCCOPY);
        }
        
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_ERASEBKGND: {
        // Only erase the viewport area, not where panels are
        RECT viewport = GetImageViewportRect();
        FillRect(reinterpret_cast<HDC>(wParam), &viewport, g_brushWindow);
        return 1;
    }

    case WM_DESTROY:
        if (g_currentImage) {
            delete g_currentImage;
            g_currentImage = nullptr;
        }
        if (g_haldCLUTImage) {
            delete g_haldCLUTImage;
            g_haldCLUTImage = nullptr;
            g_haldCLUTBitmap = nullptr;  // Same object, don't double-delete
        }
        if (g_uiFont) {
            DeleteObject(g_uiFont);
            g_uiFont = nullptr;
        }
        if (g_uiFontBold) {
            DeleteObject(g_uiFontBold);
            g_uiFontBold = nullptr;
        }
        if (g_brushWindow) {
            DeleteObject(g_brushWindow);
            g_brushWindow = nullptr;
        }
        if (g_brushPanel) {
            DeleteObject(g_brushPanel);
            g_brushPanel = nullptr;
        }
        if (g_brushHeader) {
            DeleteObject(g_brushHeader);
            g_brushHeader = nullptr;
        }
        if (g_brushHeaderHover) {
            DeleteObject(g_brushHeaderHover);
            g_brushHeaderHover = nullptr;
        }
        if (g_brushStatus) {
            DeleteObject(g_brushStatus);
            g_brushStatus = nullptr;
        }
        ClearBackbufferCache();
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR cmdLine, int cmdShow) {
    // Enable per-monitor DPI awareness for proper scaling on high-DPI displays
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    
    GdiplusStartupInput gdiplusStartupInput;
    if (GdiplusStartup(&g_gdiplusToken, &gdiplusStartupInput, nullptr) != Ok) {
        MessageBoxW(nullptr, L"Failed to initialize GDI+", L"Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    INITCOMMONCONTROLSEX icc{};
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_STANDARD_CLASSES | ICC_BAR_CLASSES | ICC_PROGRESS_CLASS;
    InitCommonControlsEx(&icc);
    
    // Load configuration
    LoadConfig();

    WNDCLASSEXW metadataClass{};
    metadataClass.cbSize = sizeof(metadataClass);
    metadataClass.lpfnWndProc = MetadataViewProc;
    metadataClass.hInstance = instance;
    metadataClass.lpszClassName = kMetadataViewClassName;
    metadataClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    metadataClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    metadataClass.hIcon = static_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(IDI_NOCTIS_VIEWER_ICON),
                                                        IMAGE_ICON, 32, 32, LR_DEFAULTCOLOR));
    metadataClass.hIconSm = static_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(IDI_NOCTIS_VIEWER_ICON),
                                                          IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR));
    // Fix: Check return value
    if (!RegisterClassExW(&metadataClass)) {
        MessageBoxW(nullptr, L"Failed to register metadata window class.", L"Error", MB_OK | MB_ICONERROR);
        GdiplusShutdown(g_gdiplusToken);
        return 1;
    }

    WNDCLASSEXW haldCLUTClass{};
    haldCLUTClass.cbSize = sizeof(haldCLUTClass);
    haldCLUTClass.lpfnWndProc = HaldCLUTViewProc;
    haldCLUTClass.hInstance = instance;
    haldCLUTClass.lpszClassName = kHaldCLUTViewClassName;
    haldCLUTClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    haldCLUTClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    haldCLUTClass.hIcon = static_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(IDI_NOCTIS_VIEWER_ICON),
                                                        IMAGE_ICON, 32, 32, LR_DEFAULTCOLOR));
    haldCLUTClass.hIconSm = static_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(IDI_NOCTIS_VIEWER_ICON),
                                                          IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR));
    if (!RegisterClassExW(&haldCLUTClass)) {
        MessageBoxW(nullptr, L"Failed to register HaldCLUT window class.", L"Error", MB_OK | MB_ICONERROR);
        GdiplusShutdown(g_gdiplusToken);
        return 1;
    }
    
    // Register progress dialog class
    WNDCLASSEXW progressClass{};
    progressClass.cbSize = sizeof(progressClass);
    progressClass.lpfnWndProc = ProgressDialogProc;
    progressClass.hInstance = instance;
    progressClass.lpszClassName = kProgressDialogClassName;
    progressClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    progressClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    if (!RegisterClassExW(&progressClass)) {
        MessageBoxW(nullptr, L"Failed to register progress dialog class.", L"Error", MB_OK | MB_ICONERROR);
        GdiplusShutdown(g_gdiplusToken);
        return 1;
    }

    WNDCLASSEXW mainClass{};
    mainClass.cbSize = sizeof(mainClass);
    mainClass.lpfnWndProc = MainWndProc;
    mainClass.hInstance = instance;
    mainClass.lpszClassName = kMainClassName;
    mainClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    mainClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    mainClass.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    mainClass.hIcon = static_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(IDI_NOCTIS_VIEWER_ICON),
                                                    IMAGE_ICON, 32, 32, LR_DEFAULTCOLOR));
    mainClass.hIconSm = static_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(IDI_NOCTIS_VIEWER_ICON),
                                                      IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR));

    if (!RegisterClassExW(&mainClass)) {
        MessageBoxW(nullptr, L"Failed to register the window class.", L"Error", MB_OK | MB_ICONERROR);
        GdiplusShutdown(g_gdiplusToken);
        return 1;
    }

    // Get system DPI to scale default window size
    UINT dpi = GetDpiForSystem();
    if (dpi == 0) dpi = 96;
    float dpiScale = static_cast<float>(dpi) / 96.0f;
    int defaultWidth = static_cast<int>(1280 * dpiScale);
    int defaultHeight = static_cast<int>(820 * dpiScale);

    HWND hwnd = CreateWindowExW(
        0, kMainClassName, kWindowTitle,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, defaultWidth, defaultHeight,
        nullptr, nullptr, instance, nullptr);

    if (!hwnd) {
        MessageBoxW(nullptr, L"Failed to create the main window.", L"Error", MB_OK | MB_ICONERROR);
        GdiplusShutdown(g_gdiplusToken);
        return 1;
    }

    ShowWindow(hwnd, cmdShow);
    UpdateWindow(hwnd);

    const std::wstring commandLine = Trim(cmdLine ? cmdLine : L"");
    if (!commandLine.empty()) {
        std::wstring initialPath = commandLine;
        // Fix: Better quote handling
        if (initialPath.size() >= 2 && initialPath.front() == L'"' && initialPath.back() == L'"') {
            initialPath = initialPath.substr(1, initialPath.size() - 2);
        }
        // Handle unquoted paths with spaces (check if it's a valid file path)
        else if (initialPath.front() == L'"') {
            // Find matching quote
            size_t endQuote = initialPath.find(L'"', 1);
            if (endQuote != std::wstring::npos) {
                initialPath = initialPath.substr(1, endQuote - 1);
            }
        }
        
        if (IsSupportedImageFile(initialPath)) {
            LoadImageFile(initialPath);
        }
    }

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    GdiplusShutdown(g_gdiplusToken);
    return static_cast<int>(msg.wParam);
}
