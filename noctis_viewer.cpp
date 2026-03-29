#include <windows.h>
#include <commctrl.h>
#include <gdiplus.h>
#include "resource.h"
#include <shellapi.h>
#include <shlwapi.h>
#include <windowsx.h>
#include <stdint.h>

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

using namespace Gdiplus;

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "shlwapi.lib")

namespace {

const wchar_t* kMainClassName = L"ComfyUIImageViewerNative";
const wchar_t* kMetadataViewClassName = L"ComfyUIMetadataView";
const wchar_t* kWindowTitle = L"Noctis Viewer";

constexpr int kMargin = 12;
constexpr int kStatusHeightFallback = 24;
constexpr int kPanelWidth = 420;
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

struct MetadataEntry {
    std::wstring key;
    std::wstring value;
    int height = 0;
};

HWND g_mainWindow = nullptr;
HWND g_statusBar = nullptr;
HWND g_metadataHeader = nullptr;
HWND g_metadataView = nullptr;

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

bool g_panelVisible = false;
bool g_panelCollapsed = false;
bool g_headerHot = false;

LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK MetadataViewProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
void ClearViewerState();
bool DeleteCurrentImage();

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
            if (compressionFlag == 0 && pos <= chunkData.size()) {
                textBytes.assign(chunkData.begin() + static_cast<long long>(pos), chunkData.end());
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
    if (!g_statusBar) {
        return kStatusHeightFallback;
    }
    return kStatusHeightFallback;
}

int GetCurrentPanelWidth() {
    if (!g_panelVisible) {
        return 0;
    }
    return g_panelCollapsed ? kCollapsedPanelWidth : kPanelWidth;
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
    } else {
        text = L"Double-click the empty area to open an image | Arrow keys navigate | Page Up/Page Down zoom";
    }
    SetWindowTextW(g_statusBar, text.c_str());
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
        const wchar_t* text = g_panelCollapsed ? L"Generation Info >>" : L"Generation Info <<";
        SetWindowTextW(g_metadataHeader, text);
        ShowWindow(g_metadataHeader, g_panelVisible ? SW_SHOW : SW_HIDE);
    }

    if (g_metadataView) {
        ShowWindow(g_metadataView, (g_panelVisible && !g_panelCollapsed) ? SW_SHOW : SW_HIDE);
        RecalculateMetadataLayout();
        InvalidateRect(g_metadataView, nullptr, FALSE);
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
    const int panelWidth = GetCurrentPanelWidth();
    const int contentHeight = std::max<int>(0, client.bottom - statusHeight);
    const int panelLeft = client.right - panelWidth;

    if (g_statusBar) {
        MoveWindow(g_statusBar, 0, client.bottom - statusHeight, client.right, statusHeight, TRUE);
    }

    if (g_metadataHeader) {
        if (g_panelVisible) {
            MoveWindow(g_metadataHeader, panelLeft, 0, panelWidth, kHeaderHeight, TRUE);
        }
        ShowWindow(g_metadataHeader, g_panelVisible ? SW_SHOW : SW_HIDE);
    }

    if (g_metadataView) {
        if (g_panelVisible && !g_panelCollapsed) {
            MoveWindow(g_metadataView, panelLeft, kHeaderHeight, panelWidth,
                       std::max(0, contentHeight - kHeaderHeight), TRUE);
        }
        ShowWindow(g_metadataView, (g_panelVisible && !g_panelCollapsed) ? SW_SHOW : SW_HIDE);
    }

    if (g_panelVisible && !g_panelCollapsed) {
        RecalculateMetadataLayout();
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
}

void LoadFolderImages(const std::wstring& folderPath) {
    g_imageFiles.clear();
    WIN32_FIND_DATAW findData{};
    HANDLE handle = FindFirstFileW((folderPath + L"\\*").c_str(), &findData);
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

        g_imageFiles.push_back(folderPath + L"\\" + name);
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
}

void ClearViewerState() {
    SetCurrentImage(nullptr);
    g_currentFilePath.clear();
    g_imageFiles.clear();
    g_metadataEntries.clear();
    g_currentIndex = -1;
    g_metadataScrollPos = 0;
    g_zoomLevel = 1.0f;
    RefreshMetadataPanel();
    UpdateWindowTitle();
    UpdateStatusBar();
    LayoutChildren(false);
    InvalidateRect(g_mainWindow, nullptr, FALSE);
}

bool LoadImageFile(const std::wstring& filePath) {
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
    FitToWindow();
    InvalidateRect(g_mainWindow, nullptr, FALSE);
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
    LoadImageFile(g_imageFiles[nextIndex]);
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
    InvalidateRect(g_mainWindow, nullptr, FALSE);
}

void ZoomOut() {
    if (!g_currentImage) {
        return;
    }
    g_zoomLevel = std::max(g_zoomLevel / kZoomStep, kMinZoom);
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

    const int x = viewport.left + std::max(0, (viewWidth - drawWidth) / 2);
    const int y = viewport.top + std::max(0, (viewHeight - drawHeight) / 2);

    Graphics graphics(dc);
    graphics.SetInterpolationMode(InterpolationModeHighQualityBicubic);
    graphics.SetPixelOffsetMode(PixelOffsetModeHighQuality);
    graphics.DrawImage(g_currentImage, x, y, drawWidth, drawHeight);
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
        FillRect(dc, &client, g_brushPanel);

        const int contentWidth = client.right - client.left;
        const int keyWidth = std::min(kKeyColumnWidth, std::max(80, contentWidth / 3));

        SetBkMode(dc, TRANSPARENT);
        HFONT oldFont = static_cast<HFONT>(SelectObject(dc, g_uiFont));

        int y = -g_metadataScrollPos;
        for (size_t i = 0; i < g_metadataEntries.size(); ++i) {
            const MetadataEntry& entry = g_metadataEntries[i];
            RECT rowRect = {0, y, contentWidth, y + entry.height};
            if (rowRect.bottom >= 0 && rowRect.top <= client.bottom) {
                HBRUSH rowBrush = CreateSolidBrush((i % 2 == 0) ? kColorPanelRowA : kColorPanelRowB);
                FillRect(dc, &rowRect, rowBrush);
                DeleteObject(rowBrush);

                RECT keyRect = {rowRect.left + kRowPaddingX, rowRect.top + kRowPaddingY,
                                rowRect.left + keyWidth - kRowPaddingX, rowRect.bottom - kRowPaddingY};
                RECT valueRect = {rowRect.left + keyWidth + kRowPaddingX, rowRect.top + kRowPaddingY,
                                  rowRect.right - kRowPaddingX, rowRect.bottom - kRowPaddingY};

                SelectObject(dc, g_uiFontBold);
                SetTextColor(dc, kColorTextSecondary);
                DrawTextW(dc, entry.key.c_str(), -1, &keyRect, DT_WORDBREAK | DT_EDITCONTROL);

                SelectObject(dc, g_uiFont);
                SetTextColor(dc, kColorTextPrimary);
                DrawTextW(dc, entry.value.c_str(), -1, &valueRect, DT_WORDBREAK | DT_EDITCONTROL);

                HPEN pen = CreatePen(PS_SOLID, 1, kColorBorder);
                HPEN oldPen = static_cast<HPEN>(SelectObject(dc, pen));
                MoveToEx(dc, keyWidth, rowRect.top, nullptr);
                LineTo(dc, keyWidth, rowRect.bottom);
                MoveToEx(dc, rowRect.left, rowRect.bottom - 1, nullptr);
                LineTo(dc, rowRect.right, rowRect.bottom - 1);
                SelectObject(dc, oldPen);
                DeleteObject(pen);
            }
            y += entry.height;
        }

        HPEN borderPen = CreatePen(PS_SOLID, 1, kColorBorder);
        HPEN oldBorderPen = static_cast<HPEN>(SelectObject(dc, borderPen));
        HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(NULL_BRUSH));
        Rectangle(dc, client.left, client.top, client.right, client.bottom);
        SelectObject(dc, oldBrush);
        SelectObject(dc, oldBorderPen);
        DeleteObject(borderPen);

        SelectObject(dc, oldFont);
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

        g_metadataHeader = CreateWindowW(
            L"BUTTON", L"Generation Info <<",
            WS_CHILD | BS_OWNERDRAW | WS_TABSTOP,
            0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(1001), nullptr, nullptr);
        SendMessageW(g_metadataHeader, WM_SETFONT, reinterpret_cast<WPARAM>(g_uiFont), TRUE);

        g_metadataView = CreateWindowExW(
            0, kMetadataViewClassName, L"",
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
            }
            return 0;
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

    case WM_GETMINMAXINFO: {
        auto* info = reinterpret_cast<MINMAXINFO*>(lParam);
        info->ptMinTrackSize.x = kMinWindowWidth;
        info->ptMinTrackSize.y = kMinWindowHeight;
        return 0;
    }

    case WM_MOUSEMOVE: {
        if (g_metadataHeader) {
            POINT point = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
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

    case WM_SIZE:
        LayoutChildren();
        FitToWindow();
        InvalidateRect(hwnd, nullptr, FALSE);
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
        if (g_panelVisible && !g_panelCollapsed && g_metadataView) {
            POINT point = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            RECT rc{};
            GetWindowRect(g_metadataView, &rc);
            if (PtInRect(&rc, point)) {
                return SendMessageW(g_metadataView, msg, wParam, lParam);
            }
        }
        if (GET_WHEEL_DELTA_WPARAM(wParam) > 0) {
            NavigateRelative(-1);
        } else {
            NavigateRelative(1);
        }
        return 0;

    case WM_KEYDOWN:
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
            ZoomIn();
            return 0;
        case VK_NEXT:
            ZoomOut();
            return 0;
        case VK_DELETE:
            DeleteCurrentImage();
            return 0;
        default:
            break;
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

    case WM_PAINT: {
        PAINTSTRUCT ps{};
        HDC dc = BeginPaint(hwnd, &ps);
        RECT client{};
        GetClientRect(hwnd, &client);

        HDC memoryDc = CreateCompatibleDC(dc);
        HBITMAP backBuffer = CreateCompatibleBitmap(dc, client.right - client.left, client.bottom - client.top);
        HBITMAP oldBitmap = static_cast<HBITMAP>(SelectObject(memoryDc, backBuffer));

        FillRect(memoryDc, &client, g_brushWindow);
        PaintImage(memoryDc);
        BitBlt(dc, 0, 0, client.right - client.left, client.bottom - client.top, memoryDc, 0, 0, SRCCOPY);

        SelectObject(memoryDc, oldBitmap);
        DeleteObject(backBuffer);
        DeleteDC(memoryDc);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_ERASEBKGND: {
        RECT client{};
        GetClientRect(hwnd, &client);
        FillRect(reinterpret_cast<HDC>(wParam), &client, g_brushWindow);
        return 1;
    }

    case WM_DESTROY:
        if (g_currentImage) {
            delete g_currentImage;
            g_currentImage = nullptr;
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
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR cmdLine, int cmdShow) {
    GdiplusStartupInput gdiplusStartupInput;
    if (GdiplusStartup(&g_gdiplusToken, &gdiplusStartupInput, nullptr) != Ok) {
        MessageBoxW(nullptr, L"Failed to initialize GDI+.", L"Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    INITCOMMONCONTROLSEX icc{};
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_STANDARD_CLASSES | ICC_BAR_CLASSES;
    InitCommonControlsEx(&icc);

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
    RegisterClassExW(&metadataClass);

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

    HWND hwnd = CreateWindowExW(
        0, kMainClassName, kWindowTitle,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1280, 820,
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
        if (initialPath.size() >= 2 && initialPath.front() == L'"' && initialPath.back() == L'"') {
            initialPath = initialPath.substr(1, initialPath.size() - 2);
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
