#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <commctrl.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <filesystem>
#include <string>
#include <vector>

#include "app_state.hpp"
#include "app_theme.hpp"
#include "link_ops.hpp"
#include "scanner.hpp"
#include "ui_layout.hpp"

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "uuid.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "uxtheme.lib")

#define IDI_APP 1

AppState g_app;
namespace fs = std::filesystem;

static bool BrowsePathDialog(HWND owner, bool isFolder, std::wstring& inOutPath, const wchar_t* title) {
    IFileOpenDialog* pDlg = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pDlg));
    if (FAILED(hr)) return false;

    DWORD options = FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST;
    if (isFolder) options |= FOS_PICKFOLDERS;
    else options |= FOS_FILEMUSTEXIST;
    pDlg->SetOptions(options);

    if (title) pDlg->SetTitle(title);

    if (!inOutPath.empty()) {
        IShellItem* pInitialItem = nullptr;
        std::wstring startDir = inOutPath;
        std::error_code ec;
        if (!isFolder && fs::is_regular_file(fs::path(startDir), ec)) {
            startDir = fs::path(startDir).parent_path().wstring();
        }
        if (SUCCEEDED(SHCreateItemFromParsingName(startDir.c_str(), nullptr, IID_PPV_ARGS(&pInitialItem)))) {
            pDlg->SetFolder(pInitialItem);
            pInitialItem->Release();
        }
    }

    hr = pDlg->Show(owner);
    if (SUCCEEDED(hr)) {
        IShellItem* pResult = nullptr;
        if (SUCCEEDED(pDlg->GetResult(&pResult))) {
            PWSTR pszPath = nullptr;
            if (SUCCEEDED(pResult->GetDisplayName(SIGDN_FILESYSPATH, &pszPath))) {
                inOutPath = pszPath;
                CoTaskMemFree(pszPath);
            }
            pResult->Release();
        }
    }
    pDlg->Release();
    return SUCCEEDED(hr);
}

static bool SaveFileDialog(HWND owner, std::wstring& outPath, const wchar_t* defaultName) {
    IFileSaveDialog* pDlg = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_FileSaveDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pDlg));
    if (FAILED(hr)) return false;

    COMDLG_FILTERSPEC filters[] = {
        { L"CSV Files (*.csv)", L"*.csv" },
        { L"All Files (*.*)", L"*.*" }
    };
    pDlg->SetFileTypes(ARRAYSIZE(filters), filters);
    pDlg->SetDefaultExtension(L"csv");
    pDlg->SetFileName(defaultName);
    pDlg->SetTitle(L"Export Discovered Links");

    hr = pDlg->Show(owner);
    if (SUCCEEDED(hr)) {
        IShellItem* pResult = nullptr;
        if (SUCCEEDED(pDlg->GetResult(&pResult))) {
            PWSTR pszPath = nullptr;
            if (SUCCEEDED(pResult->GetDisplayName(SIGDN_FILESYSPATH, &pszPath))) {
                outPath = pszPath;
                CoTaskMemFree(pszPath);
            }
            pResult->Release();
        }
    }
    pDlg->Release();
    return SUCCEEDED(hr);
}

static void SetStatusText(const std::wstring& mainText, const std::wstring& detailText = L"") {
    std::wstring full = mainText;
    if (!detailText.empty()) full += L"  " + detailText;
    SetWindowTextW(g_app.infoStatus, full.c_str());
    InvalidateRect(g_app.infoStatus, nullptr, TRUE);
}

static LRESULT CALLBACK PanelWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_COMMAND:
        return SendMessageW(GetParent(hwnd), WM_COMMAND, wParam, lParam);

    case WM_NOTIFY:
        return SendMessageW(GetParent(hwnd), WM_NOTIFY, wParam, lParam);

    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX:
    case WM_CTLCOLORBTN:
        return SendMessageW(GetParent(hwnd), uMsg, wParam, lParam);

    case WM_ERASEBKGND: {
        HDC hdc = reinterpret_cast<HDC>(wParam);
        RECT rc = {};
        GetClientRect(hwnd, &rc);
        FillRect(hdc, &rc, g_app.brushBackground);
        return 1;
    }
    }
    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CREATE: {
        g_app.hwnd = hwnd;
        HICON hIconBig = (HICON)LoadImageW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDI_APP), IMAGE_ICON, GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), LR_DEFAULTCOLOR);
        HICON hIconSmall = (HICON)LoadImageW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDI_APP), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR);
        if (hIconBig) SendMessageW(hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(hIconBig));
        if (hIconSmall) SendMessageW(hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(hIconSmall));

        Theme::Init(hwnd);
        Theme::EnableDarkTitleBar(hwnd);
        UiLayout::CreateControls(hwnd);

        if (!LinkOps::IsDeveloperMode() && !LinkOps::IsAdmin()) {
            SetStatusText(L"Notice:", L"Symbolic link creation may require Administrator privileges unless Developer Mode is on.");
        }
        return 0;
    }

    case WM_SIZE: {
        int width = LOWORD(lParam);
        int height = HIWORD(lParam);
        UiLayout::Resize(width, height);
        return 0;
    }

    case WM_DPICHANGED: {
        RECT* prcNewWindow = reinterpret_cast<RECT*>(lParam);
        Theme::Init(hwnd);
        SetWindowPos(
            hwnd, nullptr,
            prcNewWindow->left, prcNewWindow->top,
            prcNewWindow->right - prcNewWindow->left,
            prcNewWindow->bottom - prcNewWindow->top,
            SWP_NOZORDER | SWP_NOACTIVATE
        );
        return 0;
    }

    case WM_DROPFILES: {
        UiLayout::HandleDropFiles(reinterpret_cast<HDROP>(wParam));
        return 0;
    }

    case WM_COMMAND: {
        int id = LOWORD(wParam);
        int code = HIWORD(wParam);

        if ((id == ID_LINK_FOLDER_EDIT || id == ID_DEST_EDIT || id == ID_LINK_NAME_EDIT) && code == EN_CHANGE) {
            UiLayout::UpdateControlStates();
            return 0;
        }

        if (id == ID_NAV_CREATE) {
            UiLayout::SwitchPage(0);
            return 0;
        }
        if (id == ID_NAV_VIEW) {
            UiLayout::SwitchPage(1);
            return 0;
        }

        if (id == ID_CREATE_TYPE || id == ID_TARGET_TYPE) {
            if (code == CBN_SELCHANGE) {
                UiLayout::UpdateControlStates();
            }
            return 0;
        }

        if (id == ID_FILTER_TYPE) {
            if (code == CBN_SELCHANGE) {
                UiLayout::ApplyFilter();
            }
            return 0;
        }

        if (id == ID_LINK_FOLDER_BTN || id == ID_DEST_BTN) {
            bool isFolder = (id == ID_LINK_FOLDER_BTN);
            if (!isFolder) {
                int targetKind = static_cast<int>(SendMessageW(g_app.targetTypeCombo, CB_GETCURSEL, 0, 0));
                isFolder = (targetKind == TARGET_FOLDER);
            }

            HWND hTargetEdit = (id == ID_LINK_FOLDER_BTN) ? g_app.linkFolderEdit : g_app.destEdit;
            wchar_t currentPath[MAX_PATH] = {};
            GetWindowTextW(hTargetEdit, currentPath, MAX_PATH);
            std::wstring chosenPath = currentPath;

            const wchar_t* title = (id == ID_LINK_FOLDER_BTN)
                ? L"Select Link Location Folder"
                : (isFolder ? L"Select Destination Folder" : L"Select Destination File");

            if (BrowsePathDialog(hwnd, isFolder, chosenPath, title)) {
                SetWindowTextW(hTargetEdit, chosenPath.c_str());
                UiLayout::UpdateControlStates();
            }
            return 0;
        }

        if (id == ID_USE_NAME_BTN) {
            wchar_t destBuf[MAX_PATH] = {};
            GetWindowTextW(g_app.destEdit, destBuf, MAX_PATH);
            std::wstring dest = destBuf;
            if (!dest.empty()) {
                fs::path p(dest);
                std::wstring name = p.stem().wstring();
                if (name.empty()) name = p.filename().wstring();
                SetWindowTextW(g_app.linkNameEdit, name.c_str());
                UiLayout::UpdateControlStates();
            }
            return 0;
        }

        if (id == ID_GUIDE_BTN) {
            g_app.guideVisible = !g_app.guideVisible;
            SetWindowTextW(g_app.guideBtn, g_app.guideVisible ? L"Hide Guide  ?" : L"Show Guide  ?");
            if (g_app.guideVisible) {
                SetStatusText(L"Guide:", L"Link Folder is where the new link is placed. Destination is the target it points to.");
            } else {
                SetStatusText(L"");
            }
            return 0;
        }

        if (id == ID_CREATE_LINK_BTN) {
            wchar_t nameBuf[MAX_PATH] = {}, folderBuf[MAX_PATH] = {}, destBuf[MAX_PATH] = {};
            GetWindowTextW(g_app.linkNameEdit, nameBuf, MAX_PATH);
            GetWindowTextW(g_app.linkFolderEdit, folderBuf, MAX_PATH);
            GetWindowTextW(g_app.destEdit, destBuf, MAX_PATH);

            std::wstring name = nameBuf, folder = folderBuf, dest = destBuf;
            if (name.empty() || name.find_first_of(L"\\/:*?\"<>|") != std::wstring::npos) {
                SetStatusText(L"Invalid Name!", L"Enter a valid file/folder name without special characters.");
                return 0;
            }

            if (folder.empty() || dest.empty()) {
                SetStatusText(L"Missing Path!", L"Please select both Link Folder and Destination paths.");
                return 0;
            }

            int linkType = static_cast<int>(SendMessageW(g_app.typeCombo, CB_GETCURSEL, 0, 0));
            int targetKind = static_cast<int>(SendMessageW(g_app.targetTypeCombo, CB_GETCURSEL, 0, 0));
            bool isDir = (targetKind == TARGET_FOLDER);
            bool isRelative = (SendMessageW(g_app.relativeLinkCheck, BM_GETCHECK, 0, 0) == BST_CHECKED);

            DWORD destAttr = GetFileAttributesW(dest.c_str());
            if (destAttr == INVALID_FILE_ATTRIBUTES) {
                SetStatusText(L"Destination Not Found", LinkOps::FormatError(GetLastError()));
                return 0;
            }

            fs::path linkFullPath = fs::path(folder) / name;
            if (linkType == LINK_HARD) {
                linkFullPath += fs::path(dest).extension();
            }

            std::wstring linkStr = linkFullPath.wstring();
            if (GetFileAttributesW(linkStr.c_str()) != INVALID_FILE_ATTRIBUTES) {
                SetStatusText(L"Cannot Create Link", L"An item with that name already exists at the link location.");
                return 0;
            }

            std::wstring err;
            bool success = false;
            if (linkType == LINK_SYMBOLIC) {
                success = LinkOps::CreateSymlink(linkStr, dest, isDir, isRelative, err);
            } else if (linkType == LINK_HARD) {
                success = LinkOps::CreateHardLink(linkStr, dest, err);
            } else if (linkType == LINK_JUNCTION) {
                success = LinkOps::CreateJunction(linkStr, dest, err);
            }

            if (success) {
                SetStatusText(L"Success!", L"Link created at: " + linkStr);
                MessageBoxW(hwnd, (L"Link created successfully!\n\n" + linkStr).c_str(), L"Folder Junction", MB_OK | MB_ICONINFORMATION);
            } else {
                SetStatusText(L"Creation Failed", err);
                MessageBoxW(hwnd, (L"Failed to create link:\n\n" + err).c_str(), L"Creation Error", MB_OK | MB_ICONERROR);
            }
            return 0;
        }

        // Search page commands
        if (id == ID_SEARCH_BROWSE_BTN) {
            wchar_t pathBuf[MAX_PATH] = {};
            GetWindowTextW(g_app.searchPathEdit, pathBuf, MAX_PATH);
            std::wstring chosen = pathBuf;
            if (BrowsePathDialog(hwnd, true, chosen, L"Select Search Directory")) {
                SetWindowTextW(g_app.searchPathEdit, chosen.c_str());
            }
            return 0;
        }

        if (id == ID_SEARCH_ACTION_BTN) {
            if (Scanner::IsRunning()) {
                Scanner::Stop();
                SetWindowTextW(g_app.searchActionBtn, L"Search");
                SetWindowTextW(g_app.currentTaskEdit, L"Cancelling scan...");
            } else {
                wchar_t pathBuf[MAX_PATH] = {};
                GetWindowTextW(g_app.searchPathEdit, pathBuf, MAX_PATH);
                std::wstring searchDir = pathBuf;
                if (searchDir.empty()) searchDir = L"C:\\";

                ListView_DeleteAllItems(g_app.resultsGrid);
                SetWindowTextW(g_app.searchActionBtn, L"Cancel");
                SetWindowTextW(g_app.currentTaskEdit, L"Starting discovery...");
                Scanner::Start(searchDir);
            }
            return 0;
        }

        if (id == ID_EXPORT_BTN) {
            std::vector<LinkRow> exportCopy;
            {
                std::lock_guard<std::mutex> lock(g_app.rowsMutex);
                exportCopy = g_app.filteredRows.empty() ? g_app.rows : g_app.filteredRows;
            }

            if (exportCopy.empty()) {
                MessageBoxW(hwnd, L"No links to export. Run a search first.", L"Export", MB_OK | MB_ICONINFORMATION);
                return 0;
            }

            std::wstring exportPath;
            if (SaveFileDialog(hwnd, exportPath, L"FolderJunction_Report.csv")) {
                if (Scanner::ExportToCsv(exportPath, exportCopy)) {
                    MessageBoxW(hwnd, (L"Export completed successfully!\n\n" + exportPath).c_str(), L"Export Succeeded", MB_OK | MB_ICONINFORMATION);
                } else {
                    MessageBoxW(hwnd, L"Failed to write CSV file.", L"Export Failed", MB_OK | MB_ICONERROR);
                }
            }
            return 0;
        }

        if (id == ID_DELETE_BTN) {
            int sel = ListView_GetNextItem(g_app.resultsGrid, -1, LVNI_SELECTED);
            if (sel < 0) return 0;

            LinkRow targetRow;
            {
                std::lock_guard<std::mutex> lock(g_app.rowsMutex);
                if (sel < static_cast<int>(g_app.filteredRows.size())) {
                    targetRow = g_app.filteredRows[sel];
                }
            }

            if (targetRow.source.empty()) return 0;

            int answer = MessageBoxW(
                hwnd,
                (L"Are you sure you want to delete this link entry?\n\n" + targetRow.source +
                 L"\n\nNote: Only the link/shortcut entry is removed. The target data will NOT be deleted.").c_str(),
                L"Confirm Link Deletion",
                MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2
            );

            if (answer == IDYES) {
                std::wstring err;
                if (LinkOps::SafeDeleteLink(targetRow.source, err)) {
                    {
                        std::lock_guard<std::mutex> lock(g_app.rowsMutex);
                        // Remove from both lists
                        auto it1 = std::find_if(g_app.rows.begin(), g_app.rows.end(), [&](const LinkRow& r) { return r.source == targetRow.source; });
                        if (it1 != g_app.rows.end()) g_app.rows.erase(it1);

                        auto it2 = std::find_if(g_app.filteredRows.begin(), g_app.filteredRows.end(), [&](const LinkRow& r) { return r.source == targetRow.source; });
                        if (it2 != g_app.filteredRows.end()) g_app.filteredRows.erase(it2);
                    }
                    UiLayout::PopulateGrid();
                    SetStatusText(L"Link deleted successfully.");
                } else {
                    MessageBoxW(hwnd, (L"Failed to delete link: " + err).c_str(), L"Deletion Error", MB_OK | MB_ICONERROR);
                }
            }
            return 0;
        }

        break;
    }

    case WM_SCAN_BATCH: {
        auto* pBatch = reinterpret_cast<std::vector<LinkRow>*>(lParam);
        if (pBatch) {
            {
                std::lock_guard<std::mutex> lock(g_app.rowsMutex);
                for (auto& row : *pBatch) {
                    g_app.rows.push_back(std::move(row));
                }
            }
            delete pBatch;
            UiLayout::ApplyFilter();
        }
        return 0;
    }

    case WM_SCAN_PROGRESS: {
        auto* pStr = reinterpret_cast<std::wstring*>(lParam);
        if (pStr) {
            SetWindowTextW(g_app.currentTaskEdit, pStr->c_str());
            delete pStr;
        }
        return 0;
    }

    case WM_SCAN_DONE: {
        g_app.scanning = false;
        SetWindowTextW(g_app.searchActionBtn, L"Search");
        SetWindowTextW(g_app.currentTaskEdit, L"Discovery completed.");

        size_t count = 0;
        {
            std::lock_guard<std::mutex> lock(g_app.rowsMutex);
            count = g_app.rows.size();
        }

        std::wstring msg = g_app.cancelScan ? L"Scan cancelled. " : L"Scan completed. ";
        msg += std::to_wstring(count) + L" link(s) found.";
        SetStatusText(msg);
        return 0;
    }

    case WM_NOTIFY: {
        auto* pNmhdr = reinterpret_cast<NMHDR*>(lParam);
        if (pNmhdr && pNmhdr->idFrom == ID_RESULTS_GRID) {
            if (pNmhdr->code == LVN_ITEMCHANGED) {
                int sel = ListView_GetNextItem(g_app.resultsGrid, -1, LVNI_SELECTED);
                EnableWindow(g_app.deleteLinkBtn, sel >= 0);
            } else if (pNmhdr->code == NM_DBLCLK) {
                int sel = ListView_GetNextItem(g_app.resultsGrid, -1, LVNI_SELECTED);
                if (sel >= 0) {
                    wchar_t pathBuf[MAX_PATH] = {};
                    ListView_GetItemText(g_app.resultsGrid, sel, 0, pathBuf, MAX_PATH);
                    std::wstring arg = L"/select,\"" + std::wstring(pathBuf) + L"\"";
                    ShellExecuteW(hwnd, L"open", L"explorer.exe", arg.c_str(), nullptr, SW_SHOWNORMAL);
                }
            } else if (pNmhdr->code == NM_RCLICK) {
                auto* pItem = reinterpret_cast<NMITEMACTIVATE*>(lParam);
                POINT pt = pItem->ptAction;
                ClientToScreen(g_app.resultsGrid, &pt);
                UiLayout::ShowContextMenu(hwnd, pt.x, pt.y);
            }
        }
        break;
    }

    case WM_CTLCOLORSTATIC: {
        HDC hdc = reinterpret_cast<HDC>(wParam);
        HWND hCtrl = reinterpret_cast<HWND>(lParam);

        if (hCtrl == g_app.publisherLabel) {
            SetTextColor(hdc, Theme::COLOR_MUTED);
            SetBkColor(hdc, Theme::COLOR_BG);
            return reinterpret_cast<LRESULT>(g_app.brushBackground);
        }

        SetTextColor(hdc, Theme::COLOR_FG);

        wchar_t className[32] = {};
        GetClassNameW(hCtrl, className, 32);
        if (_wcsicmp(className, L"EDIT") == 0) {
            SetBkColor(hdc, Theme::COLOR_FIELD);
            return reinterpret_cast<LRESULT>(g_app.brushField);
        }

        SetBkColor(hdc, Theme::COLOR_BG);
        return reinterpret_cast<LRESULT>(g_app.brushBackground);
    }

    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX: {
        HDC hdc = reinterpret_cast<HDC>(wParam);
        SetTextColor(hdc, Theme::COLOR_FG);
        SetBkColor(hdc, Theme::COLOR_FIELD);
        return reinterpret_cast<LRESULT>(g_app.brushField);
    }

    case WM_CTLCOLORBTN: {
        HDC hdc = reinterpret_cast<HDC>(wParam);
        SetTextColor(hdc, Theme::COLOR_FG);
        SetBkColor(hdc, Theme::COLOR_BG);
        return reinterpret_cast<LRESULT>(g_app.brushBackground);
    }

    case WM_ERASEBKGND: {
        HDC hdc = reinterpret_cast<HDC>(wParam);
        RECT rc = {};
        GetClientRect(hwnd, &rc);
        FillRect(hdc, &rc, g_app.brushBackground);
        return 1;
    }

    case WM_DESTROY: {
        Scanner::Stop();
        Theme::Cleanup();
        PostQuitMessage(0);
        return 0;
    }
    }

    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow) {
    // Enable Per-Monitor v2 DPI awareness programmatically if supported
    typedef BOOL(WINAPI* PFN_SetProcessDpiAwarenessContext)(DPI_AWARENESS_CONTEXT);
    auto pfnSetDpiContext = reinterpret_cast<PFN_SetProcessDpiAwarenessContext>(
        GetProcAddress(GetModuleHandleW(L"user32.dll"), "SetProcessDpiAwarenessContext")
    );
    if (pfnSetDpiContext) {
        pfnSetDpiContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    }

    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    INITCOMMONCONTROLSEX icc = { sizeof(INITCOMMONCONTROLSEX) };
    icc.dwICC = ICC_WIN95_CLASSES | ICC_TAB_CLASSES | ICC_LISTVIEW_CLASSES | ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icc);

    WNDCLASSEXW wcPanel = { sizeof(WNDCLASSEXW) };
    wcPanel.lpfnWndProc = PanelWndProc;
    wcPanel.hInstance = hInstance;
    wcPanel.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wcPanel.hbrBackground = nullptr;
    wcPanel.lpszClassName = L"FolderJunctionPanel";
    RegisterClassExW(&wcPanel);

    HICON hIconBig = (HICON)LoadImageW(hInstance, MAKEINTRESOURCEW(IDI_APP), IMAGE_ICON, GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), LR_DEFAULTCOLOR);
    HICON hIconSmall = (HICON)LoadImageW(hInstance, MAKEINTRESOURCEW(IDI_APP), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR);

    WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hIcon = hIconBig ? hIconBig : LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_APP));
    wc.hIconSm = hIconSmall ? hIconSmall : wc.hIcon;
    wc.hbrBackground = nullptr;
    wc.lpszClassName = L"FolderJunctionMainWindow";
    RegisterClassExW(&wc);

    int defaultW = 760;
    int defaultH = 480;
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int posX = (screenW - defaultW) / 2;
    int posY = (screenH - defaultH) / 2;

    HWND hwnd = CreateWindowExW(
        0,
        wc.lpszClassName,
        L"Folder Junction v1.0",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        posX, posY, defaultW, defaultH,
        nullptr, nullptr, hInstance, nullptr
    );

    if (!hwnd) {
        CoUninitialize();
        return 1;
    }

    if (hIconBig) SendMessageW(hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(hIconBig));
    if (hIconSmall) SendMessageW(hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(hIconSmall));

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg = {};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    CoUninitialize();
    return static_cast<int>(msg.wParam);
}
