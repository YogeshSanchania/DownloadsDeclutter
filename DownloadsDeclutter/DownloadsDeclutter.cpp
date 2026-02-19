// DownloadsDeclutter.cpp : Final Version with Real-time Status Updates
//

#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "shlwapi.lib")

#include <windows.h>
#include <ShlObj.h>
#include <shellapi.h>
#include <uxtheme.h>
#include <shlwapi.h> 
#include <string>
#include <map>
#include <vector>
#include <mutex>
#include <commctrl.h>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <ctime>
#include <iomanip>
#include <set>

#include "framework.h"
#include "DownloadsDeclutter.h"

#define MAX_LOADSTRING 100

// Control IDs
#define ID_BTN_SCAN        1001
#define ID_BTN_PREVIEW     1002
#define ID_BTN_ORGANIZE    1003
#define ID_CMB_FOLDERSTRAT 1004
#define ID_CHK_NEWER       1005
#define ID_CMB_DAYS        1006
#define ID_BTN_CANCEL      1010
#define ID_BTN_UNDO        1011
#define ID_LISTVIEW        1020
#define ID_STATUSBAR       1030
#define ID_EDIT_PATH       1040
#define ID_BTN_BROWSE      1041
#define ID_BTN_ABOUT_OK    1060

// Menu IDs
#define IDM_EXIT           1050
#define IDM_ABOUT          1051

// Custom Messages
#define WM_SCAN_UPDATE   (WM_USER + 1)
#define WM_SCAN_COMPLETE (WM_USER + 2)
#define WM_ORG_PROGRESS  (WM_USER + 3)
#define WM_ORG_COMPLETE  (WM_USER + 4)
#define WM_UNDO_COMPLETE (WM_USER + 5)

// Global Font
HFONT g_hAppFont = nullptr;
HFONT g_hBoldFont = nullptr;

enum class FolderStrategy
{
    Ignore, Flatten, MoveAsItem
};
enum class FileCategory
{
    Documents, Images, Videos, Audio, Archives, Installers, TextFiles, CodeFiles, Folders, Others
};

struct CategoryStats
{
    unsigned long long fileCount = 0; unsigned long long totalSize = 0;
};

struct FileEntry
{
    std::wstring fileName;
    std::wstring fullPath;
    unsigned long long size;
    FileCategory category;
    int iIcon;
    bool isDirectory = false;
};

struct HistoryEntry
{
    std::wstring originalPath;
    std::wstring movedPath;
    std::wstring batchId;
    std::wstring timestamp;
};

struct AppContext
{
    std::wstring folderPath;
    std::wstring cliArgPath;
    HWND hWnd;

    FolderStrategy folderStrategy = FolderStrategy::Ignore;
    bool skipNewerFiles = false;
    int  skipDaysCount = 3;

    std::mutex dataMutex;
    bool cancelRequested = false;
    bool isWorking = false;

    // Data
    unsigned long long fileCount = 0;
    unsigned long long totalSize = 0;
    std::map<FileCategory, CategoryStats> categoryMap;
    std::vector<FileEntry> files;

    // Stats
    int movedCount = 0;
    int errorCount = 0;
    int undoCount = 0;
};

// Global Variables
HINSTANCE hInst;
WCHAR szTitle[MAX_LOADSTRING];
WCHAR szWindowClass[MAX_LOADSTRING];
WCHAR szAboutClass[] = L"DownloadsDeclutterAbout";

HWND g_hListView = nullptr;
HWND g_hStatusBar = nullptr;
HWND g_hComboDays = nullptr;
HWND g_hComboStrat = nullptr;
HWND g_hBtnUndo = nullptr;
HWND g_hPathEdit = nullptr;
HWND g_hBtnBrowse = nullptr;

AppContext g_ctx;

// Forward declarations
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK    AboutWndProc(HWND, UINT, WPARAM, LPARAM);
void                SetStandardFont(HWND hWnd);
void                OpenSelectedFile(HWND hWnd);
bool                IsSystemPath(const std::wstring& path);
void                UpdateHistoryUI(HWND hWnd);
void                ShowAboutDialog(HWND hParent);

// --- UTF-8 Encoding Helpers ---
std::string ToUtf8(const std::wstring& wstr)
{
    if (wstr.empty()) return std::string();
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
    return strTo;
}

std::wstring FromUtf8(const std::string& str)
{
    if (str.empty()) return std::wstring();
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
    return wstrTo;
}

FileCategory GetFileCategory(const std::wstring& fileName, bool isDir)
{
    if (isDir) return FileCategory::Folders;
    size_t pos = fileName.find_last_of(L'.');
    if (pos == std::wstring::npos) return FileCategory::Others;
    std::wstring ext = fileName.substr(pos + 1);
    for (auto& c : ext) c = towlower(c);

    if (ext == L"pdf" || ext == L"doc" || ext == L"docx" || ext == L"xls" || ext == L"xlsx" || ext == L"ppt" || ext == L"pptx" || ext == L"csv") return FileCategory::Documents;
    if (ext == L"txt" || ext == L"md" || ext == L"log" || ext == L"rtf") return FileCategory::TextFiles;
    if (ext == L"cpp" || ext == L"h" || ext == L"py" || ext == L"js" || ext == L"ts" || ext == L"html" || ext == L"css" || ext == L"java" || ext == L"cs" || ext == L"json" || ext == L"xml" || ext == L"sql") return FileCategory::CodeFiles;
    if (ext == L"jpg" || ext == L"jpeg" || ext == L"png" || ext == L"gif" || ext == L"bmp" || ext == L"webp" || ext == L"svg" || ext == L"ico" || ext == L"tiff" || ext == L"heic") return FileCategory::Images;
    if (ext == L"mp4" || ext == L"mkv" || ext == L"avi" || ext == L"mov" || ext == L"webm" || ext == L"flv" || ext == L"wmv") return FileCategory::Videos;
    if (ext == L"mp3" || ext == L"wav" || ext == L"aac" || ext == L"flac" || ext == L"ogg" || ext == L"m4a") return FileCategory::Audio;
    if (ext == L"zip" || ext == L"rar" || ext == L"7z" || ext == L"tar" || ext == L"gz" || ext == L"iso") return FileCategory::Archives;
    if (ext == L"exe" || ext == L"msi" || ext == L"bat" || ext == L"cmd" || ext == L"sh") return FileCategory::Installers;
    return FileCategory::Others;
}

std::wstring CategoryToString(FileCategory c)
{
    switch (c)
    {
    case FileCategory::Documents:  return L"Documents";
    case FileCategory::Images:     return L"Images";
    case FileCategory::Videos:     return L"Videos";
    case FileCategory::Audio:      return L"Audio";
    case FileCategory::Archives:   return L"Archives";
    case FileCategory::TextFiles:  return L"Text Files";
    case FileCategory::CodeFiles:  return L"Code Files";
    case FileCategory::Installers: return L"Installers";
    case FileCategory::Folders:    return L"Folders";
    default:                       return L"Others";
    }
}

std::wstring FormatFileSize(unsigned long long sizeInBytes)
{
    const wchar_t* suffixes[] = { L"B", L"KB", L"MB", L"GB", L"TB" };
    size_t suffixIndex = 0;
    double size = static_cast<double>(sizeInBytes);
    while (size >= 1024.0 && suffixIndex < _countof(suffixes) - 1)
    {
        size /= 1024.0; suffixIndex++;
    }
    wchar_t buffer[64]; swprintf_s(buffer, L"%.2f %s", size, suffixes[suffixIndex]); return std::wstring(buffer);
}

bool IsSystemPath(const std::wstring& path)
{
    wchar_t sysPath[MAX_PATH];
    GetWindowsDirectoryW(sysPath, MAX_PATH); if (path.find(sysPath) == 0) return true;
    GetSystemDirectoryW(sysPath, MAX_PATH); if (path.find(sysPath) == 0) return true;
    SHGetFolderPathW(NULL, CSIDL_PROGRAM_FILES, NULL, 0, sysPath); if (path.find(sysPath) == 0) return true;
    SHGetFolderPathW(NULL, CSIDL_PROGRAM_FILESX86, NULL, 0, sysPath); if (path.find(sysPath) == 0) return true;
    return false;
}

bool IsFileNewerThan(const FILETIME& fileTime, int days)
{
    FILETIME now; GetSystemTimeAsFileTime(&now);
    ULARGE_INTEGER ullNow, ullFile;
    ullNow.LowPart = now.dwLowDateTime; ullNow.HighPart = now.dwHighDateTime;
    ullFile.LowPart = fileTime.dwLowDateTime; ullFile.HighPart = fileTime.dwHighDateTime;
    if (ullFile.QuadPart > ullNow.QuadPart) return true;
    return ((ullNow.QuadPart - ullFile.QuadPart) < ((unsigned long long)days * 864000000000ULL));
}

// --- History Helpers ---

std::wstring GetHistoryFilePath(const std::wstring& folder)
{
    wchar_t buffer[MAX_PATH];
    PathCombineW(buffer, folder.c_str(), L".declutter_history");
    return std::wstring(buffer);
}

void LogMove(const std::wstring& folder, const std::wstring& batchId, const std::wstring& src, const std::wstring& dest)
{
    std::wstring path = GetHistoryFilePath(folder);
    SetFileAttributesW(path.c_str(), FILE_ATTRIBUTE_NORMAL);
    std::ofstream logFile(path, std::ios::app | std::ios::binary);
    time_t now = time(0); struct tm tstruct; localtime_s(&tstruct, &now);
    wchar_t timeBuf[80]; wcsftime(timeBuf, 80, L"%Y-%m-%d %H:%M:%S", &tstruct);
    std::wstring fullLine = batchId + L"|" + timeBuf + L"|" + src + L"|" + dest;
    std::string utf8Line = ToUtf8(fullLine) + "\r\n";
    logFile.write(utf8Line.c_str(), utf8Line.size());
    logFile.close();
    SetFileAttributesW(path.c_str(), FILE_ATTRIBUTE_HIDDEN);
}

std::pair<std::wstring, std::wstring> GetLastBatchInfo(const std::wstring& folder)
{
    std::wstring path = GetHistoryFilePath(folder);
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return { L"", L"" };
    std::string lineUtf8;
    std::wstring lastBatch, lastTime;
    while (std::getline(file, lineUtf8))
    {
        if (!lineUtf8.empty() && lineUtf8.back() == '\r') lineUtf8.pop_back();
        if (lineUtf8.empty()) continue;
        std::wstring line = FromUtf8(lineUtf8);
        size_t p1 = line.find(L'|');
        if (p1 != std::wstring::npos)
        {
            lastBatch = line.substr(0, p1);
            size_t p2 = line.find(L'|', p1 + 1);
            if (p2 != std::wstring::npos) lastTime = line.substr(p1 + 1, p2 - p1 - 1);
        }
    }
    return { lastBatch, lastTime };
}

// --- Scanning ---

void ScanRecursive(const std::wstring& folderPath, AppContext* ctx, std::map<std::wstring, int>& iconCache, std::vector<FileEntry>& localBatch)
{
    WIN32_FIND_DATAW findData;
    std::wstring searchPath = folderPath + L"\\*";
    HANDLE hFind = FindFirstFileW(searchPath.c_str(), &findData);
    if (hFind == INVALID_HANDLE_VALUE) return;

    do
    {
        if (ctx->cancelRequested) break;
        const std::wstring fileName = findData.cFileName;
        if (fileName == L"." || fileName == L"..") continue;

        std::wstring fullPath = folderPath + L"\\" + fileName;
        bool isDir = (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);

        if (isDir && (fileName == L"Windows" || fileName == L"Program Files" || fileName == L"Program Files (x86)")) continue;

        if (isDir)
        {
            if (fileName == L".git" || fileName == L".vs" || fileName == L"node_modules" || fileName == L".declutter_history") continue;
            if (ctx->folderStrategy == FolderStrategy::Flatten) ScanRecursive(fullPath, ctx, iconCache, localBatch);
            else if (ctx->folderStrategy == FolderStrategy::MoveAsItem)
            {
                if (ctx->skipNewerFiles && IsFileNewerThan(findData.ftLastWriteTime, ctx->skipDaysCount)) continue;
                FileEntry entry = { fileName, fullPath, 0, FileCategory::Folders, 0, true };
                SHFILEINFO sfi = { 0 }; SHGetFileInfo(fullPath.c_str(), FILE_ATTRIBUTE_DIRECTORY, &sfi, sizeof(sfi), SHGFI_SYSICONINDEX | SHGFI_SMALLICON | SHGFI_USEFILEATTRIBUTES);
                entry.iIcon = sfi.iIcon;
                localBatch.push_back(std::move(entry));
            }
        }
        else
        {
            if (fileName == L".declutter_history") continue;
            if (ctx->skipNewerFiles && IsFileNewerThan(findData.ftLastWriteTime, ctx->skipDaysCount)) continue;
            FileEntry entry; entry.fileName = fileName; entry.fullPath = fullPath;
            entry.size = ((unsigned long long)findData.nFileSizeHigh << 32) | findData.nFileSizeLow;
            entry.category = GetFileCategory(fileName, false);
            entry.isDirectory = false;
            size_t dotPos = fileName.find_last_of(L'.');
            std::wstring ext = (dotPos != std::wstring::npos) ? fileName.substr(dotPos) : L"";
            for (auto& c : ext) c = towlower(c);
            if (iconCache.find(ext) != iconCache.end()) entry.iIcon = iconCache[ext];
            else
            {
                SHFILEINFO sfi = { 0 }; SHGetFileInfo(ext.empty() ? L"file" : ext.c_str(), FILE_ATTRIBUTE_NORMAL, &sfi, sizeof(sfi), SHGFI_SYSICONINDEX | SHGFI_SMALLICON | SHGFI_USEFILEATTRIBUTES);
                entry.iIcon = sfi.iIcon; iconCache[ext] = sfi.iIcon;
            }
            localBatch.push_back(std::move(entry));
        }

        if (localBatch.size() >= 500)
        {
            std::lock_guard<std::mutex> lock(ctx->dataMutex);
            ctx->files.insert(ctx->files.end(), std::make_move_iterator(localBatch.begin()), std::make_move_iterator(localBatch.end()));
            for (const auto& f : localBatch)
            {
                ctx->fileCount++; ctx->totalSize += f.size; ctx->categoryMap[f.category].fileCount++; ctx->categoryMap[f.category].totalSize += f.size;
            }
            localBatch.clear(); PostMessage(ctx->hWnd, WM_SCAN_UPDATE, 0, 0);
        }
    } while (FindNextFileW(hFind, &findData) != 0);
    FindClose(hFind);
}

DWORD WINAPI ScanThreadProc(LPVOID lpParam)
{
    AppContext* ctx = (AppContext*)lpParam;
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    std::map<std::wstring, int> iconCache; std::vector<FileEntry> localBatch; localBatch.reserve(1024);
    ScanRecursive(ctx->folderPath, ctx, iconCache, localBatch);
    if (!localBatch.empty())
    {
        std::lock_guard<std::mutex> lock(ctx->dataMutex);
        ctx->files.insert(ctx->files.end(), std::make_move_iterator(localBatch.begin()), std::make_move_iterator(localBatch.end()));
        for (const auto& f : localBatch)
        {
            ctx->fileCount++; ctx->totalSize += f.size; ctx->categoryMap[f.category].fileCount++; ctx->categoryMap[f.category].totalSize += f.size;
        }
    }
    CoUninitialize();
    PostMessage(ctx->hWnd, WM_SCAN_COMPLETE, 0, 0);
    return 0;
}

std::wstring GetUniqueDestinationPath(const std::wstring& folder, const std::wstring& fileName)
{
    std::wstring destPath = folder + L"\\" + fileName;
    if (GetFileAttributesW(destPath.c_str()) == INVALID_FILE_ATTRIBUTES) return destPath;
    std::wstring name, ext;
    size_t dotPos = fileName.find_last_of(L'.');
    if (dotPos == std::wstring::npos)
    {
        name = fileName;
    }
    else
    {
        name = fileName.substr(0, dotPos); ext = fileName.substr(dotPos);
    }
    int i = 1;
    while (true)
    {
        std::wstring newName = name + L" (" + std::to_wstring(i) + L")" + ext;
        destPath = folder + L"\\" + newName;
        if (GetFileAttributesW(destPath.c_str()) == INVALID_FILE_ATTRIBUTES) return destPath;
        i++; if (i > 100) return L"";
    }
}

DWORD WINAPI OrganizeThreadProc(LPVOID lpParam)
{
    AppContext* ctx = (AppContext*)lpParam;
    ctx->movedCount = 0; ctx->errorCount = 0;
    std::vector<FileEntry> filesCopy;
    {
        std::lock_guard<std::mutex> lock(ctx->dataMutex); filesCopy = ctx->files;
    }

    time_t now = time(0); std::wstring batchId = std::to_wstring(now);
    int total = (int)filesCopy.size(); int current = 0;
    for (const auto& file : filesCopy)
    {
        if (ctx->cancelRequested) break;
        current++;

        std::wstring catFolder = CategoryToString(file.category);
        std::wstring targetDir = ctx->folderPath + L"\\" + catFolder;
        if (file.isDirectory && file.fullPath == targetDir) continue;

        if (GetFileAttributesW(targetDir.c_str()) == INVALID_FILE_ATTRIBUTES) CreateDirectoryW(targetDir.c_str(), NULL);
        std::wstring destPath = GetUniqueDestinationPath(targetDir, file.fileName);

        if (!destPath.empty())
        {
            if (MoveFileW(file.fullPath.c_str(), destPath.c_str()))
            {
                ctx->movedCount++;
                LogMove(ctx->folderPath, batchId, file.fullPath, destPath);
            }
            else ctx->errorCount++;
        }
        else ctx->errorCount++;
    }
    PostMessage(ctx->hWnd, WM_ORG_COMPLETE, 0, 0);
    return 0;
}

DWORD WINAPI UndoThreadProc(LPVOID lpParam)
{
    AppContext* ctx = (AppContext*)lpParam;
    ctx->undoCount = 0; ctx->errorCount = 0;

    std::wstring histPath = GetHistoryFilePath(ctx->folderPath);
    SetFileAttributesW(histPath.c_str(), FILE_ATTRIBUTE_NORMAL);

    std::vector<std::wstring> lines;
    try
    {
        std::ifstream infile(histPath, std::ios::binary);
        if (infile.is_open())
        {
            std::string lineUtf8;
            while (std::getline(infile, lineUtf8))
            {
                if (!lineUtf8.empty() && lineUtf8.back() == '\r') lineUtf8.pop_back();
                if (!lineUtf8.empty()) lines.push_back(FromUtf8(lineUtf8));
            }
            infile.close();
        }
    }
    catch (...)
    {
        SetFileAttributesW(histPath.c_str(), FILE_ATTRIBUTE_HIDDEN);
        PostMessage(ctx->hWnd, WM_UNDO_COMPLETE, 0, 0);
        return 0;
    }

    if (lines.empty())
    {
        SetFileAttributesW(histPath.c_str(), FILE_ATTRIBUTE_HIDDEN);
        PostMessage(ctx->hWnd, WM_UNDO_COMPLETE, 0, 0);
        return 0;
    }

    std::wstring lastBatchId;
    try
    {
        if (!lines.empty())
        {
            size_t pipe = lines.back().find(L'|');
            if (pipe != std::wstring::npos) lastBatchId = lines.back().substr(0, pipe);
        }
    }
    catch (...)
    {
    }

    if (lastBatchId.empty())
    {
        SetFileAttributesW(histPath.c_str(), FILE_ATTRIBUTE_HIDDEN);
        PostMessage(ctx->hWnd, WM_UNDO_COMPLETE, 0, 0);
        return 0;
    }

    std::vector<std::wstring> remainingLines;
    std::vector<HistoryEntry> toUndo;

    for (const auto& ln : lines)
    {
        bool match = false;
        try
        {
            if (ln.find(lastBatchId + L"|") == 0) match = true;
        }
        catch (...)
        {
        }

        if (match)
        {
            try
            {
                size_t p1 = ln.find(L'|'); size_t p2 = ln.find(L'|', p1 + 1); size_t p3 = ln.find(L'|', p2 + 1);
                if (p1 != std::wstring::npos && p2 != std::wstring::npos && p3 != std::wstring::npos)
                {
                    HistoryEntry e; e.originalPath = ln.substr(p2 + 1, p3 - p2 - 1); e.movedPath = ln.substr(p3 + 1);
                    toUndo.push_back(e);
                }
            }
            catch (...)
            {
            }
        }
        else remainingLines.push_back(ln);
    }

    std::set<std::wstring> foldersToClean;
    for (int i = (int)toUndo.size() - 1; i >= 0; i--)
    {
        const auto& item = toUndo[i];
        size_t lastSlash = item.movedPath.find_last_of(L'\\');
        if (lastSlash != std::wstring::npos) foldersToClean.insert(item.movedPath.substr(0, lastSlash));

        if (GetFileAttributesW(item.movedPath.c_str()) != INVALID_FILE_ATTRIBUTES)
        {
            if (GetFileAttributesW(item.originalPath.c_str()) == INVALID_FILE_ATTRIBUTES)
            {
                if (MoveFileW(item.movedPath.c_str(), item.originalPath.c_str())) ctx->undoCount++;
                else ctx->errorCount++;
            }
            else ctx->errorCount++;
        }
    }

    for (const auto& folder : foldersToClean) RemoveDirectoryW(folder.c_str());

    if (remainingLines.empty()) DeleteFileW(histPath.c_str());
    else
    {
        std::ofstream outfile(histPath, std::ios::trunc | std::ios::binary);
        for (const auto& ln : remainingLines)
        {
            std::string utf8Line = ToUtf8(ln) + "\r\n";
            outfile.write(utf8Line.c_str(), utf8Line.size());
        }
        outfile.close();
        SetFileAttributesW(histPath.c_str(), FILE_ATTRIBUTE_HIDDEN);
    }

    PostMessage(ctx->hWnd, WM_UNDO_COMPLETE, 0, 0);
    return 0;
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow)
{
    int nArgs;
    LPWSTR* szArglist = CommandLineToArgvW(GetCommandLineW(), &nArgs);
    if (szArglist && nArgs > 1)
    {
        g_ctx.cliArgPath = szArglist[1];
    }
    LocalFree(szArglist);

    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_DOWNLOADSDECLUTTER, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);
    INITCOMMONCONTROLSEX icex = { sizeof(icex), ICC_LISTVIEW_CLASSES | ICC_BAR_CLASSES | ICC_STANDARD_CLASSES | ICC_LINK_CLASS };
    InitCommonControlsEx(&icex);

    g_hAppFont = CreateFontW(19, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    g_hBoldFont = CreateFontW(22, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");

    if (!InitInstance(hInstance, nCmdShow)) return FALSE;
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg); DispatchMessage(&msg);
    }
    DeleteObject(g_hAppFont);
    DeleteObject(g_hBoldFont);
    return (int)msg.wParam;
}

ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex = { sizeof(WNDCLASSEX) };
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON2));
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = nullptr;
    wcex.lpszClassName = szWindowClass;
    wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_ICON2));
    RegisterClassExW(&wcex);

    WNDCLASSEXW wcexAbout = { sizeof(WNDCLASSEX) };
    wcexAbout.lpfnWndProc = AboutWndProc;
    wcexAbout.hInstance = hInstance;
    wcexAbout.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcexAbout.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcexAbout.lpszClassName = szAboutClass;
    return RegisterClassExW(&wcexAbout);
}

BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
    hInst = hInstance;
    HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, 0, 950, 700, nullptr, nullptr, hInstance, nullptr);
    if (!hWnd) return FALSE;

    HMENU hMenuBar = CreateMenu();
    HMENU hFileMenu = CreatePopupMenu();
    AppendMenuW(hFileMenu, MF_STRING, IDM_EXIT, L"E&xit");
    HMENU hHelpMenu = CreatePopupMenu();
    AppendMenuW(hHelpMenu, MF_STRING, IDM_ABOUT, L"&About");
    AppendMenuW(hMenuBar, MF_POPUP, (UINT_PTR)hFileMenu, L"&File");
    AppendMenuW(hMenuBar, MF_POPUP, (UINT_PTR)hHelpMenu, L"&Help");
    SetMenu(hWnd, hMenuBar);

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);
    return TRUE;
}

std::wstring GetTargetFolderPath()
{
    if (!g_ctx.cliArgPath.empty()) return g_ctx.cliArgPath;
    PWSTR path = nullptr;
    SHGetKnownFolderPath(FOLDERID_Downloads, 0, nullptr, &path);
    std::wstring p = path ? path : L"";
    if (path) CoTaskMemFree(path);
    return p;
}

void ResizeControls(HWND hWnd)
{
    RECT rc; GetClientRect(hWnd, &rc);
    int width = rc.right - rc.left; int height = rc.bottom - rc.top;

    //SendMessage(g_hStatusBar, WM_SIZE, 0, 0);
    SendMessage(g_hStatusBar, WM_SIZE, 0, 0);
    int parts[] = { width - 300, -1 };
    SendMessage(g_hStatusBar, SB_SETPARTS, 2, (LPARAM)parts);


    RECT rcStatus; GetWindowRect(g_hStatusBar, &rcStatus); int statusHeight = rcStatus.bottom - rcStatus.top;

    MoveWindow(g_hPathEdit, 80, 10, width - 140, 24, TRUE);
    MoveWindow(g_hBtnBrowse, width - 50, 10, 30, 24, TRUE);
    MoveWindow(g_hListView, 20, 160, width - 40, height - 160 - statusHeight, TRUE);
}

void SetStandardFont(HWND hWnd)
{
    SendMessage(hWnd, WM_SETFONT, (WPARAM)g_hAppFont, TRUE);
}

void UpdateHistoryUI(HWND hWnd)
{
    auto info = GetLastBatchInfo(g_ctx.folderPath);
    std::wstring txt = L"Last organized: Never";
    bool canUndo = false;
    if (!info.second.empty())
    {
        txt = L"Last organized: " + info.second; canUndo = true;
    }

    SendMessage(g_hStatusBar, SB_SETTEXT, 1, (LPARAM)txt.c_str());
    EnableWindow(g_hBtnUndo, canUndo);
}

void OpenSelectedFile(HWND hWnd)
{
    int iItem = (int)SendMessage(g_hListView, LVM_GETNEXTITEM, -1, LVNI_SELECTED);
    if (iItem == -1) return;
    std::wstring fullPath;
    {
        std::lock_guard<std::mutex> lock(g_ctx.dataMutex);
        if (iItem < (int)g_ctx.files.size())
            fullPath = g_ctx.files[iItem].fullPath;
    }
    if (!fullPath.empty()) 
        ShellExecuteW(hWnd, L"open", fullPath.c_str(), NULL, NULL, SW_SHOW);
}

void ShowAboutDialog(HWND hParent)
{
    RECT rc; GetWindowRect(hParent, &rc);
    int width = 350; int height = 220;
    int x = rc.left + (rc.right - rc.left - width) / 2;
    int y = rc.top + (rc.bottom - rc.top - height) / 2;

    HWND hAbout = CreateWindowExW(WS_EX_DLGMODALFRAME | WS_EX_TOPMOST, szAboutClass, L"About",
        WS_VISIBLE | WS_POPUP | WS_CAPTION | WS_SYSMENU, x, y, width, height, hParent, nullptr, hInst, nullptr);
    EnableWindow(hParent, FALSE);
}

LRESULT CALLBACK AboutWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
    {
        HICON hIconLarge = (HICON)LoadImageW(hInst, MAKEINTRESOURCE(IDI_ICON2), IMAGE_ICON, 64, 64, LR_DEFAULTCOLOR | LR_SHARED);
        HWND hIconStatic = CreateWindowW(L"STATIC", NULL, WS_CHILD | WS_VISIBLE | SS_ICON | SS_CENTERIMAGE,
            20, 40, 64, 64, hWnd, nullptr, hInst, nullptr);
        SendMessage(hIconStatic, STM_SETICON, (WPARAM)hIconLarge, 0);

        HWND hTitle = CreateWindowW(L"STATIC", L"Downloads Declutter", WS_CHILD | WS_VISIBLE | SS_LEFT,
            100, 30, 200, 25, hWnd, nullptr, hInst, nullptr);
        SendMessage(hTitle, WM_SETFONT, (WPARAM)g_hBoldFont, TRUE);

        HWND hVer = CreateWindowW(L"STATIC", L"Version 1.0", WS_CHILD | WS_VISIBLE | SS_LEFT,
            100, 55, 200, 20, hWnd, nullptr, hInst, nullptr);
        SendMessage(hVer, WM_SETFONT, (WPARAM)g_hAppFont, TRUE);

        HWND hLink = CreateWindowW(WC_LINK,
            L"Developed by <a href=\"https://www.linkedin.com/in/yogesh-sanchania\">Yogesh Sanchania</a>",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            100, 80, 200, 20, hWnd, nullptr, hInst, nullptr);
        SendMessage(hLink, WM_SETFONT, (WPARAM)g_hAppFont, TRUE);

        HWND hBtn = CreateWindowW(L"BUTTON", L"OK", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
            230, 130, 80, 30, hWnd, (HMENU)ID_BTN_ABOUT_OK, hInst, nullptr);
        SendMessage(hBtn, WM_SETFONT, (WPARAM)g_hAppFont, TRUE);
        break;
    }
    case WM_NOTIFY:
    {
        LPNMHDR pnmh = (LPNMHDR)lParam;
        if (pnmh->code == NM_CLICK || pnmh->code == NM_RETURN)
        {
            PNMLINK pNMLink = (PNMLINK)lParam;
            ShellExecuteW(NULL, L"open", pNMLink->item.szUrl, NULL, NULL, SW_SHOW);
        }
        break;
    }
    case WM_COMMAND:
        if (LOWORD(wParam) == ID_BTN_ABOUT_OK || LOWORD(wParam) == IDCANCEL)
            DestroyWindow(hWnd);
        break;
    case WM_CLOSE:
        DestroyWindow(hWnd);
        break;
    case WM_DESTROY:
        EnableWindow(GetWindow(hWnd, GW_OWNER), TRUE);
        SetForegroundWindow(GetWindow(hWnd, GW_OWNER));
        break;
    default: return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
    {
        CreateWindowW(L"STATIC", L"Target:", WS_CHILD | WS_VISIBLE, 20, 12, 60, 20, hWnd, nullptr, hInst, nullptr);
        g_hPathEdit = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 80, 10, 600, 24, hWnd, (HMENU)ID_EDIT_PATH, hInst, nullptr);
        g_hBtnBrowse = CreateWindowW(L"BUTTON", L"...", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 690, 10, 30, 24, hWnd, (HMENU)ID_BTN_BROWSE, hInst, nullptr);

        std::wstring startPath = GetTargetFolderPath();
        SetWindowTextW(g_hPathEdit, startPath.c_str());
        g_ctx.folderPath = startPath;
        
        
        CreateWindowW(L"STATIC", L"Subfolder Strategy:", WS_CHILD | WS_VISIBLE, 20, 45, 120, 20, hWnd, nullptr, hInst, nullptr);
        g_hComboStrat = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST, 140, 42, 200, 200, hWnd, (HMENU)ID_CMB_FOLDERSTRAT, hInst, nullptr);
        SendMessage(g_hComboStrat, CB_ADDSTRING, 0, (LPARAM)L"Ignore Subfolders");
        SendMessage(g_hComboStrat, CB_ADDSTRING, 0, (LPARAM)L"Organize Contents");
        SendMessage(g_hComboStrat, CB_ADDSTRING, 0, (LPARAM)L"Move Folders to 'Folders'");
        SendMessage(g_hComboStrat, CB_SETCURSEL, 0, 0);

        CreateWindowW(L"BUTTON", L"Skip newer than", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 360, 45, 130, 24, hWnd, (HMENU)ID_CHK_NEWER, hInst, nullptr);
        g_hComboDays = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST, 495, 46, 50, 200, hWnd, (HMENU)ID_CMB_DAYS, hInst, nullptr);
        for (int i = 1; i <= 10; i++)
        {
            wchar_t buf[10]; swprintf_s(buf, L"%d", i); SendMessage(g_hComboDays, CB_ADDSTRING, 0, (LPARAM)buf);
        }
        SendMessage(g_hComboDays, CB_SETCURSEL, 2, 0);
        CreateWindowW(L"STATIC", L"days", WS_CHILD | WS_VISIBLE, 550, 48, 50, 20, hWnd, nullptr, hInst, nullptr);

        CreateWindowW(L"BUTTON", L"Scan", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 20, 90, 100, 35, hWnd, (HMENU)ID_BTN_SCAN, hInst, nullptr);
        CreateWindowW(L"BUTTON", L"Preview", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_DISABLED, 130, 90, 100, 35, hWnd, (HMENU)ID_BTN_PREVIEW, hInst, nullptr);
        CreateWindowW(L"BUTTON", L"Organize", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_DISABLED, 240, 90, 100, 35, hWnd, (HMENU)ID_BTN_ORGANIZE, hInst, nullptr);
        g_hBtnUndo = CreateWindowW(L"BUTTON", L"Undo Last", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_DISABLED, 350, 90, 100, 35, hWnd, (HMENU)ID_BTN_UNDO, hInst, nullptr);
        CreateWindowW(L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_DISABLED, 460, 90, 80, 35, hWnd, (HMENU)ID_BTN_CANCEL, hInst, nullptr);

        g_hListView = CreateWindowW(WC_LISTVIEW, L"", WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_OWNERDATA | LVS_SHAREIMAGELISTS, 20, 150, 800, 300, hWnd, (HMENU)ID_LISTVIEW, hInst, nullptr);
        ListView_SetExtendedListViewStyle(g_hListView, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);

        LVCOLUMNW col = {}; col.mask = LVCF_TEXT | LVCF_WIDTH;
        col.cx = 450; col.pszText = (LPWSTR)L"File Name"; ListView_InsertColumn(g_hListView, 0, &col);
        col.cx = 100; col.pszText = (LPWSTR)L"Size";      ListView_InsertColumn(g_hListView, 1, &col);
        col.cx = 150; col.pszText = (LPWSTR)L"Category";  ListView_InsertColumn(g_hListView, 2, &col);

        SHFILEINFO sfi = { 0 }; HIMAGELIST hSysImgList = (HIMAGELIST)SHGetFileInfo(L"C:\\", 0, &sfi, sizeof(sfi), SHGFI_SYSICONINDEX | SHGFI_SMALLICON);
        ListView_SetImageList(g_hListView, hSysImgList, LVSIL_SMALL);

        int parts[] = { 600, -1 };
        g_hStatusBar = CreateWindowW(STATUSCLASSNAME, nullptr, WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP, 0, 0, 0, 0, hWnd, (HMENU)ID_STATUSBAR, hInst, nullptr);
        SendMessage(g_hStatusBar, SB_SETPARTS, 2, (LPARAM)parts);
        SendMessage(g_hStatusBar, SB_SETTEXT, 0, (LPARAM)L"Ready.");
		
        EnumChildWindows(hWnd, [](HWND hChild, LPARAM) -> BOOL
            {
                SetStandardFont(hChild); return TRUE;
            }, 0);
        UpdateHistoryUI(hWnd);
        break;
    }
    case WM_SIZE: ResizeControls(hWnd); break;

    case WM_COMMAND:
    {
        int id = LOWORD(wParam);
        int code = HIWORD(wParam);

        // --- REAL-TIME STATUS UPDATE ---
        if (id == ID_EDIT_PATH && code == EN_CHANGE)
        {
            wchar_t pathBuf[MAX_PATH];
            GetWindowTextW(g_hPathEdit, pathBuf, MAX_PATH);
            g_ctx.folderPath = pathBuf;
            UpdateHistoryUI(hWnd);
            return 0;
        }

        switch (id)
        {
        case IDM_EXIT: DestroyWindow(hWnd); break;
        case IDM_ABOUT: ShowAboutDialog(hWnd); break;

        case ID_BTN_SCAN:
        {
            if (g_ctx.isWorking) break;

            wchar_t pathBuf[MAX_PATH];
            GetWindowTextW(g_hPathEdit, pathBuf, MAX_PATH);
            std::wstring userPath = pathBuf;

            DWORD attrs = GetFileAttributesW(userPath.c_str());
            if (attrs == INVALID_FILE_ATTRIBUTES || !(attrs & FILE_ATTRIBUTE_DIRECTORY))
            {
                MessageBoxW(hWnd, L"The selected path does not exist or is not a directory.", L"Invalid Path", MB_OK | MB_ICONERROR);
                break;
            }
            if (IsSystemPath(userPath))
            {
                MessageBoxW(hWnd, L"System paths (Windows, Program Files) are protected for safety.", L"Unsafe Path", MB_OK | MB_ICONERROR);
                break;
            }

            g_ctx.folderPath = userPath;
            UpdateHistoryUI(hWnd);

            {
                std::lock_guard<std::mutex> lock(g_ctx.dataMutex); g_ctx.files.clear(); g_ctx.categoryMap.clear(); g_ctx.fileCount = 0; g_ctx.totalSize = 0;
            }
            g_ctx.cancelRequested = false; g_ctx.isWorking = true; g_ctx.hWnd = hWnd;

            int sIdx = (int)SendMessage(g_hComboStrat, CB_GETCURSEL, 0, 0);
            if (sIdx == 1) g_ctx.folderStrategy = FolderStrategy::Flatten;
            else if (sIdx == 2) g_ctx.folderStrategy = FolderStrategy::MoveAsItem;
            else g_ctx.folderStrategy = FolderStrategy::Ignore;

            g_ctx.skipNewerFiles = (SendMessage(GetDlgItem(hWnd, ID_CHK_NEWER), BM_GETCHECK, 0, 0) == BST_CHECKED);
            g_ctx.skipDaysCount = (int)SendMessage(g_hComboDays, CB_GETCURSEL, 0, 0) + 1;

            ListView_SetItemCount(g_hListView, 0);
            EnableWindow(GetDlgItem(hWnd, ID_BTN_SCAN), FALSE); EnableWindow(GetDlgItem(hWnd, ID_BTN_ORGANIZE), FALSE); EnableWindow(GetDlgItem(hWnd, ID_BTN_UNDO), FALSE); EnableWindow(GetDlgItem(hWnd, ID_BTN_CANCEL), TRUE);
            SendMessage(g_hStatusBar, SB_SETTEXT, 0, (LPARAM)L"Scanning...");
            CreateThread(nullptr, 0, ScanThreadProc, &g_ctx, 0, nullptr);
            break;
        }
        case ID_BTN_CANCEL:
            g_ctx.cancelRequested = true;
            SendMessage(g_hStatusBar, SB_SETTEXT, 0, (LPARAM)L"Stopping...");
            break;
        case ID_BTN_PREVIEW: OpenSelectedFile(hWnd); break;
        case ID_BTN_ORGANIZE:
        {
            if (g_ctx.isWorking) break;
            if (MessageBoxW(hWnd, L"Organize files?", L"Confirm", MB_YESNO | MB_ICONQUESTION) == IDNO) return 0;
            g_ctx.isWorking = true; g_ctx.cancelRequested = false;
            EnableWindow(GetDlgItem(hWnd, ID_BTN_SCAN), FALSE); EnableWindow(GetDlgItem(hWnd, ID_BTN_ORGANIZE), FALSE); EnableWindow(GetDlgItem(hWnd, ID_BTN_UNDO), FALSE); EnableWindow(GetDlgItem(hWnd, ID_BTN_CANCEL), TRUE);
            SendMessage(g_hStatusBar, SB_SETTEXT, 0, (LPARAM)L"Organizing...");
            CreateThread(nullptr, 0, OrganizeThreadProc, &g_ctx, 0, nullptr);
            break;
        }
        case ID_BTN_UNDO:
        {
            if (g_ctx.isWorking) break;
            auto info = GetLastBatchInfo(g_ctx.folderPath);
            std::wstring msg = L"Undo the last batch from " + info.second + L"?\n\nNote: If you have renamed or moved files manually since then, those specific files cannot be restored.";
            if (MessageBoxW(hWnd, msg.c_str(), L"Confirm Undo", MB_YESNO | MB_ICONWARNING) == IDNO) return 0;
            g_ctx.isWorking = true; g_ctx.cancelRequested = false; g_ctx.hWnd = hWnd;
            EnableWindow(GetDlgItem(hWnd, ID_BTN_SCAN), FALSE); EnableWindow(GetDlgItem(hWnd, ID_BTN_ORGANIZE), FALSE); EnableWindow(GetDlgItem(hWnd, ID_BTN_UNDO), FALSE);
            SendMessage(g_hStatusBar, SB_SETTEXT, 0, (LPARAM)L"Undoing...");
            CreateThread(nullptr, 0, UndoThreadProc, &g_ctx, 0, nullptr);
            break;
        }
        case ID_BTN_BROWSE:
        {
            wchar_t szDisplayName[MAX_PATH] = { 0 };
            BROWSEINFOW bi = {};
            bi.hwndOwner = hWnd;
            bi.lpszTitle = L"Select target folder";
            bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
            bi.pszDisplayName = szDisplayName;
            LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
            if (pidl)
            {
                wchar_t selectedPath[MAX_PATH] = { 0 };
                if (SHGetPathFromIDListW(pidl, selectedPath))
                {
                    // Update textbox -> Triggers EN_CHANGE -> Updates Context/UI
                    SetWindowTextW(g_hPathEdit, selectedPath);

                    // Force update just in case SetWindowText didn't trigger EN_CHANGE 
                    g_ctx.folderPath = selectedPath;
                    UpdateHistoryUI(hWnd);

                    // Auto-scan
                    SendMessage(hWnd, WM_COMMAND, ID_BTN_SCAN, 0);
                }
                CoTaskMemFree(pidl);
            }
            break;
        }
        }
        break;
    }
    case WM_NOTIFY:
    {
        LPNMHDR phdr = (LPNMHDR)lParam;
        if (phdr->idFrom == ID_LISTVIEW && phdr->code == LVN_GETDISPINFO)
        {
            NMLVDISPINFO* plvdi = (NMLVDISPINFO*)lParam; int index = plvdi->item.iItem;
            std::lock_guard<std::mutex> lock(g_ctx.dataMutex);
            if (index >= 0 && index < (int)g_ctx.files.size())
            {
                const FileEntry& entry = g_ctx.files[index];
                if (plvdi->item.mask & LVIF_TEXT)
                {
                    if (plvdi->item.iSubItem == 0) wcsncpy_s(plvdi->item.pszText, plvdi->item.cchTextMax, entry.fileName.c_str(), _TRUNCATE);
                    else if (plvdi->item.iSubItem == 1) wcsncpy_s(plvdi->item.pszText, plvdi->item.cchTextMax, entry.isDirectory ? L"<DIR>" : FormatFileSize(entry.size).c_str(), _TRUNCATE);
                    else if (plvdi->item.iSubItem == 2) wcsncpy_s(plvdi->item.pszText, plvdi->item.cchTextMax, CategoryToString(entry.category).c_str(), _TRUNCATE);
                }
                if (plvdi->item.mask & LVIF_IMAGE) plvdi->item.iImage = entry.iIcon;
            }
        }
        if (phdr->idFrom == ID_LISTVIEW && phdr->code == NM_DBLCLK) OpenSelectedFile(hWnd);
        break;
    }

    case WM_SCAN_UPDATE:
    {
        size_t c = 0;
        {
            std::lock_guard<std::mutex> lock(g_ctx.dataMutex); c = g_ctx.files.size();
        } ListView_SetItemCountEx(g_hListView, c, LVSICF_NOINVALIDATEALL); wchar_t buf[128]; swprintf_s(buf, L"Scanning: %llu items...", c); SendMessage(g_hStatusBar, SB_SETTEXT, 0, (LPARAM)buf); break;
    }
    case WM_SCAN_COMPLETE:
    {
        g_ctx.isWorking = false;
        {
            std::lock_guard<std::mutex> lock(g_ctx.dataMutex); ListView_SetItemCount(g_hListView, g_ctx.files.size());
        } EnableWindow(GetDlgItem(hWnd, ID_BTN_SCAN), TRUE); EnableWindow(GetDlgItem(hWnd, ID_BTN_PREVIEW), TRUE); EnableWindow(GetDlgItem(hWnd, ID_BTN_ORGANIZE), TRUE); EnableWindow(GetDlgItem(hWnd, ID_BTN_CANCEL), FALSE); wchar_t buf[256]; swprintf_s(buf, L"Scan Complete. Found %llu items.", g_ctx.files.size()); SendMessage(g_hStatusBar, SB_SETTEXT, 0, (LPARAM)buf); UpdateHistoryUI(hWnd); break;
    }
    case WM_ORG_COMPLETE:
    {
        g_ctx.isWorking = false; EnableWindow(GetDlgItem(hWnd, ID_BTN_SCAN), TRUE); EnableWindow(GetDlgItem(hWnd, ID_BTN_ORGANIZE), TRUE); EnableWindow(GetDlgItem(hWnd, ID_BTN_CANCEL), FALSE); wchar_t buf[256]; swprintf_s(buf, L"Done! Moved: %d. Errors: %d", g_ctx.movedCount, g_ctx.errorCount); MessageBoxW(hWnd, buf, L"Success", MB_OK | MB_ICONINFORMATION); SendMessage(g_hStatusBar, SB_SETTEXT, 0, (LPARAM)buf); UpdateHistoryUI(hWnd); SendMessage(hWnd, WM_COMMAND, ID_BTN_SCAN, 0); break;
    }
    case WM_UNDO_COMPLETE:
    {
        g_ctx.isWorking = false; EnableWindow(GetDlgItem(hWnd, ID_BTN_SCAN), TRUE); EnableWindow(GetDlgItem(hWnd, ID_BTN_ORGANIZE), TRUE); EnableWindow(GetDlgItem(hWnd, ID_BTN_CANCEL), FALSE); wchar_t buf[256]; swprintf_s(buf, L"Undo Complete. Restored: %d. Errors/Skipped: %d", g_ctx.undoCount, g_ctx.errorCount); MessageBoxW(hWnd, buf, L"Undo Result", MB_OK | MB_ICONINFORMATION); UpdateHistoryUI(hWnd); SendMessage(hWnd, WM_COMMAND, ID_BTN_SCAN, 0); break;
    }
    case WM_DESTROY: PostQuitMessage(0); break;
    default: return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}