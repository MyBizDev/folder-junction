#include "ui_layout.hpp"
#include "app_theme.hpp"
#include "link_ops.hpp"
#include "scanner.hpp"
#include <shellapi.h>
#include <shobjidl.h>
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;

namespace UiLayout {

static HWND CreateChildControl(
    HWND parent, const wchar_t* className, const wchar_t* text,
    DWORD style, int x, int y, int w, int h, int id
) {
    HWND hCtrl = CreateWindowExW(
        0, className, text, WS_CHILD | WS_VISIBLE | style,
        Theme::Scale(x), Theme::Scale(y), Theme::Scale(w), Theme::Scale(h),
        parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
        GetModuleHandleW(nullptr), nullptr
    );
    Theme::ApplyControlStyle(hCtrl);
    return hCtrl;
}

static void AddTooltip(HWND hTooltip, HWND hCtrl, const wchar_t* text) {
    if (!hTooltip || !hCtrl || !text) return;
    TOOLINFOW ti = { sizeof(TOOLINFOW) };
    ti.uFlags = TTF_SUBCLASS | TTF_IDISHWND;
    ti.hwnd = GetParent(hCtrl);
    ti.uId = reinterpret_cast<UINT_PTR>(hCtrl);
    ti.lpszText = const_cast<LPWSTR>(text);
    SendMessageW(hTooltip, TTM_ADDTOOLW, 0, reinterpret_cast<LPARAM>(&ti));
}

void CreateControls(HWND parent) {
    // Title
    HWND hTitle = CreateChildControl(parent, L"STATIC", L"Folder Junction", SS_LEFT, 14, 20, 130, 28, ID_APP_TITLE);
    Theme::SetControlFont(hTitle, true);

    // Sidebar navigation buttons
    g_app.navCreate = CreateChildControl(parent, L"BUTTON", L"Create", BS_PUSHBUTTON | WS_TABSTOP, 10, 68, 125, 36, ID_NAV_CREATE);
    g_app.navView   = CreateChildControl(parent, L"BUTTON", L"View", BS_PUSHBUTTON | WS_TABSTOP, 10, 110, 125, 36, ID_NAV_VIEW);

    // Publisher message at the bottom of the sidebar
    g_app.publisherLabel = CreateChildControl(parent, L"STATIC", L"Publisher:\nAbhishek Chaurasia", SS_LEFT, 12, 380, 120, 36, ID_PUBLISHER_LABEL);
    if (g_app.fontStatus) {
        SendMessageW(g_app.publisherLabel, WM_SETFONT, reinterpret_cast<WPARAM>(g_app.fontStatus), TRUE);
    }

    // Container pages
    g_app.createPage = CreateWindowExW(0, L"FolderJunctionPanel", L"", WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
        Theme::Scale(145), 0, Theme::Scale(530), Theme::Scale(440), parent, nullptr, GetModuleHandleW(nullptr), nullptr);
    g_app.viewPage = CreateWindowExW(0, L"FolderJunctionPanel", L"", WS_CHILD | WS_CLIPCHILDREN,
        Theme::Scale(145), 0, Theme::Scale(530), Theme::Scale(440), parent, nullptr, GetModuleHandleW(nullptr), nullptr);

    // === CREATE PAGE ===
    CreateChildControl(g_app.createPage, L"STATIC", L"Create a", 0, 12, 12, 90, 20, ID_CREATE_TYPE_LABEL);
    g_app.typeCombo = CreateChildControl(g_app.createPage, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, 12, 34, 150, 180, ID_CREATE_TYPE);
    SendMessageW(g_app.typeCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Symbolic Link"));
    SendMessageW(g_app.typeCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Hard Link"));
    SendMessageW(g_app.typeCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Junction Link"));
    SendMessageW(g_app.typeCombo, CB_SETCURSEL, 0, 0);

    CreateChildControl(g_app.createPage, L"STATIC", L"for a", 0, 175, 12, 60, 20, ID_TARGET_TYPE_LABEL);
    g_app.targetTypeCombo = CreateChildControl(g_app.createPage, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, 175, 34, 120, 180, ID_TARGET_TYPE);
    SendMessageW(g_app.targetTypeCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Folder"));
    SendMessageW(g_app.targetTypeCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"File"));
    SendMessageW(g_app.targetTypeCombo, CB_SETCURSEL, 0, 0);

    g_app.relativeLinkCheck = CreateChildControl(g_app.createPage, L"BUTTON", L"Relative link", BS_AUTOCHECKBOX | WS_TABSTOP, 310, 36, 110, 24, ID_REL_LINK_CHECK);

    g_app.guideBtn = CreateChildControl(g_app.createPage, L"BUTTON", L"Show Guide  ?", BS_PUSHBUTTON | WS_TABSTOP, 430, 28, 110, 32, ID_GUIDE_BTN);

    // Link Folder row
    g_app.linkFolderLabel = CreateChildControl(g_app.createPage, L"STATIC", L"Link Folder", 0, 12, 72, 220, 20, ID_LINK_FOLDER_LABEL);
    g_app.linkFolderEdit  = CreateChildControl(g_app.createPage, L"EDIT", L"", ES_AUTOHSCROLL | WS_TABSTOP | WS_BORDER, 12, 94, 400, 32, ID_LINK_FOLDER_EDIT);
    g_app.linkFolderBrowseBtn = CreateChildControl(g_app.createPage, L"BUTTON", L"Browse...", BS_PUSHBUTTON | WS_TABSTOP, 420, 94, 100, 32, ID_LINK_FOLDER_BTN);

    // Destination row
    g_app.destLabel = CreateChildControl(g_app.createPage, L"STATIC", L"Destination Folder", 0, 12, 136, 220, 20, ID_DEST_LABEL);
    g_app.destEdit  = CreateChildControl(g_app.createPage, L"EDIT", L"", ES_AUTOHSCROLL | WS_TABSTOP | WS_BORDER, 12, 158, 400, 32, ID_DEST_EDIT);
    g_app.destBrowseBtn = CreateChildControl(g_app.createPage, L"BUTTON", L"Browse...", BS_PUSHBUTTON | WS_TABSTOP, 420, 158, 100, 32, ID_DEST_BTN);

    // Link Name row
    g_app.linkNameEdit = CreateChildControl(g_app.createPage, L"EDIT", L"", ES_AUTOHSCROLL | WS_TABSTOP | WS_BORDER, 12, 206, 250, 32, ID_LINK_NAME_EDIT);
    SendMessageW(g_app.linkNameEdit, EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(L"Enter link name"));
    g_app.useDestNameBtn = CreateChildControl(g_app.createPage, L"BUTTON", L"Use Destination Name", BS_PUSHBUTTON | WS_TABSTOP, 272, 206, 180, 32, ID_USE_NAME_BTN);

    // Status label and Action button
    g_app.infoStatus = CreateChildControl(g_app.createPage, L"STATIC", L"", SS_LEFT, 12, 255, 380, 36, ID_INFO_LABEL);
    g_app.createLinkBtn = CreateChildControl(g_app.createPage, L"BUTTON", L"Create Link", BS_DEFPUSHBUTTON | WS_TABSTOP, 400, 252, 120, 38, ID_CREATE_LINK_BTN);
    EnableWindow(g_app.createLinkBtn, FALSE);

    // === VIEW PAGE ===
    g_app.searchPathLabel = CreateChildControl(g_app.viewPage, L"STATIC", L"Search Directory:", 0, 12, 10, 150, 20, ID_SEARCH_PATH_LABEL);
    g_app.searchPathEdit  = CreateChildControl(g_app.viewPage, L"EDIT", g_app.searchRoot.c_str(), ES_AUTOHSCROLL | WS_BORDER | WS_TABSTOP, 12, 32, 280, 32, ID_SEARCH_PATH_EDIT);
    g_app.searchBrowseBtn = CreateChildControl(g_app.viewPage, L"BUTTON", L"Browse", BS_PUSHBUTTON | WS_TABSTOP, 300, 32, 80, 32, ID_SEARCH_BROWSE_BTN);
    g_app.searchActionBtn = CreateChildControl(g_app.viewPage, L"BUTTON", L"Search", BS_DEFPUSHBUTTON | WS_TABSTOP, 390, 32, 90, 32, ID_SEARCH_ACTION_BTN);

    g_app.filterCombo = CreateChildControl(g_app.viewPage, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, 490, 32, 120, 180, ID_FILTER_TYPE);
    SendMessageW(g_app.filterCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"All Types"));
    SendMessageW(g_app.filterCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"SymLinks Only"));
    SendMessageW(g_app.filterCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Junctions Only"));
    SendMessageW(g_app.filterCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Shortcuts Only"));
    SendMessageW(g_app.filterCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Broken Links Only"));
    SendMessageW(g_app.filterCombo, CB_SETCURSEL, 0, 0);

    // Results ListView
    g_app.resultsGrid = CreateWindowExW(
        WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | WS_TABSTOP,
        Theme::Scale(12), Theme::Scale(74), Theme::Scale(600), Theme::Scale(260),
        g_app.viewPage, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_RESULTS_GRID)),
        GetModuleHandleW(nullptr), nullptr
    );
    Theme::ApplyControlStyle(g_app.resultsGrid);
    ListView_SetBkColor(g_app.resultsGrid, Theme::COLOR_PANEL);
    ListView_SetTextBkColor(g_app.resultsGrid, Theme::COLOR_PANEL);
    ListView_SetTextColor(g_app.resultsGrid, Theme::COLOR_FG);
    ListView_SetExtendedListViewStyle(g_app.resultsGrid, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_GRIDLINES);

    const wchar_t* columnHeaders[] = { L"Source Path", L"Type", L"Target Path", L"Target Exists" };
    int initialWidths[] = { 240, 80, 240, 80 };
    for (int i = 0; i < 4; ++i) {
        LVCOLUMNW col = {};
        col.mask = LVCF_TEXT | LVCF_WIDTH;
        col.pszText = const_cast<LPWSTR>(columnHeaders[i]);
        col.cx = Theme::Scale(initialWidths[i]);
        ListView_InsertColumn(g_app.resultsGrid, i, &col);
    }

    // Bottom Task & Actions
    g_app.currentTaskLabel = CreateChildControl(g_app.viewPage, L"STATIC", L"Scanning:", 0, 12, 342, 100, 18, ID_TASK_LABEL);
    g_app.currentTaskEdit  = CreateChildControl(g_app.viewPage, L"EDIT", L"", ES_READONLY | ES_AUTOHSCROLL | WS_BORDER, 12, 362, 360, 28, ID_TASK_EDIT);

    g_app.exportBtn     = CreateChildControl(g_app.viewPage, L"BUTTON", L"Export CSV...", BS_PUSHBUTTON | WS_TABSTOP, 385, 360, 105, 32, ID_EXPORT_BTN);
    g_app.deleteLinkBtn = CreateChildControl(g_app.viewPage, L"BUTTON", L"Delete Link", BS_PUSHBUTTON | WS_TABSTOP, 500, 360, 110, 32, ID_DELETE_BTN);
    EnableWindow(g_app.deleteLinkBtn, FALSE);

    // Drag and Drop support
    DragAcceptFiles(parent, TRUE);

    // Initialize Rich Tooltips
    g_app.tooltip = CreateWindowExW(
        WS_EX_TOPMOST, TOOLTIPS_CLASS, nullptr,
        WS_POPUP | TTS_ALWAYSTIP | TTS_BALLOON,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        parent, nullptr, GetModuleHandleW(nullptr), nullptr
    );
    if (g_app.tooltip) {
        SendMessageW(g_app.tooltip, TTM_SETMAXTIPWIDTH, 0, static_cast<LPARAM>(Theme::Scale(350)));

        // Create page tooltips
        AddTooltip(g_app.tooltip, g_app.linkFolderLabel, L"Link Folder: The folder where the new link or junction will be placed and accessed in File Explorer.");
        AddTooltip(g_app.tooltip, g_app.linkFolderEdit, L"Link Folder: The folder where the new link or junction will be placed and accessed in File Explorer.");
        AddTooltip(g_app.tooltip, g_app.linkFolderBrowseBtn, L"Browse and select the folder where the link will be created.");

        AddTooltip(g_app.tooltip, g_app.destLabel, L"Destination Target: The real folder or file where the actual data is physically stored.");
        AddTooltip(g_app.tooltip, g_app.destEdit, L"Destination Target: The real folder or file where the actual data is physically stored.");
        AddTooltip(g_app.tooltip, g_app.destBrowseBtn, L"Browse and select the target folder or file containing the actual data.");

        AddTooltip(g_app.tooltip, g_app.linkNameEdit, L"Link Name: The name for the newly created link object (can differ from destination name).");
        AddTooltip(g_app.tooltip, g_app.useDestNameBtn, L"Automatically copy the destination folder or file name into the link name.");

        AddTooltip(g_app.tooltip, g_app.relativeLinkCheck, L"Relative Link: Create a relative symbolic link portable across volume/folder moves.");
        AddTooltip(g_app.tooltip, g_app.typeCombo, L"Link Type: Symbolic Link (flexible pointer), Hard Link (same volume, file only), or Junction (NTFS folder mount point).");
        AddTooltip(g_app.tooltip, g_app.targetTypeCombo, L"Target Kind: Choose whether the link points to a Folder or a File.");
        AddTooltip(g_app.tooltip, g_app.guideBtn, L"Toggle the visual guide explaining link locations vs destination targets.");
        AddTooltip(g_app.tooltip, g_app.createLinkBtn, L"Create the link at the specified Link Folder pointing to the Destination Target.");

        // View page tooltips
        AddTooltip(g_app.tooltip, g_app.searchPathEdit, L"Search Root: The folder path where discovery will search for links, junctions, and shortcuts.");
        AddTooltip(g_app.tooltip, g_app.searchBrowseBtn, L"Browse and select a folder to scan.");
        AddTooltip(g_app.tooltip, g_app.searchActionBtn, L"Start or cancel scanning for links and junctions.");
        AddTooltip(g_app.tooltip, g_app.filterCombo, L"Filter the results list by link type or find broken/dangling links.");
        AddTooltip(g_app.tooltip, g_app.exportBtn, L"Export all discovered links and target existence statuses to a CSV spreadsheet.");
        AddTooltip(g_app.tooltip, g_app.deleteLinkBtn, L"Safely delete the selected link entry without deleting any data in its target folder.");
    }

    UpdateControlStates();
    SwitchPage(0);
}

void SwitchPage(int pageIndex) {
    g_app.activePage = pageIndex;
    ShowWindow(g_app.createPage, pageIndex == 0 ? SW_SHOW : SW_HIDE);
    ShowWindow(g_app.viewPage, pageIndex == 1 ? SW_SHOW : SW_HIDE);
    EnableWindow(g_app.navCreate, pageIndex != 0);
    EnableWindow(g_app.navView, pageIndex != 1);
}

void Resize(int clientWidth, int clientHeight) {
    int sidebarWidth = Theme::Scale(140);
    int contentWidth = clientWidth - sidebarWidth;
    int contentHeight = clientHeight;

    if (contentWidth < Theme::Scale(200) || contentHeight < Theme::Scale(200)) return;

    if (g_app.publisherLabel) {
        int pad = Theme::Scale(12);
        SetWindowPos(g_app.publisherLabel, nullptr, pad, clientHeight - Theme::Scale(46), sidebarWidth - pad * 2, Theme::Scale(40), SWP_NOZORDER);
    }

    SetWindowPos(g_app.createPage, nullptr, sidebarWidth, 0, contentWidth, contentHeight, SWP_NOZORDER);
    SetWindowPos(g_app.viewPage, nullptr, sidebarWidth, 0, contentWidth, contentHeight, SWP_NOZORDER);

    // Resizing View Page controls
    int pad = Theme::Scale(12);
    int topBarY = Theme::Scale(32);
    int btnH = Theme::Scale(32);
    int bottomBarY = contentHeight - Theme::Scale(55);

    int actionBtnW = Theme::Scale(90);
    int browseBtnW = Theme::Scale(80);
    int filterW = Theme::Scale(130);

    int searchEditW = contentWidth - pad * 5 - browseBtnW - actionBtnW - filterW;
    if (searchEditW < Theme::Scale(100)) searchEditW = Theme::Scale(100);

    SetWindowPos(g_app.searchPathEdit, nullptr, pad, topBarY, searchEditW, btnH, SWP_NOZORDER);
    SetWindowPos(g_app.searchBrowseBtn, nullptr, pad + searchEditW + pad / 2, topBarY, browseBtnW, btnH, SWP_NOZORDER);
    SetWindowPos(g_app.searchActionBtn, nullptr, pad + searchEditW + pad / 2 + browseBtnW + pad / 2, topBarY, actionBtnW, btnH, SWP_NOZORDER);
    SetWindowPos(g_app.filterCombo, nullptr, contentWidth - pad - filterW, topBarY, filterW, btnH, SWP_NOZORDER);

    int gridTop = Theme::Scale(74);
    int gridHeight = bottomBarY - gridTop - Theme::Scale(12);
    int gridWidth = contentWidth - pad * 2;
    if (gridHeight > 0 && gridWidth > 0) {
        SetWindowPos(g_app.resultsGrid, nullptr, pad, gridTop, gridWidth, gridHeight, SWP_NOZORDER);

        // Adjust column widths proportionally
        int colType = Theme::Scale(85);
        int colValid = Theme::Scale(90);
        int pathColWidth = (gridWidth - colType - colValid - Theme::Scale(25)) / 2;
        if (pathColWidth > Theme::Scale(100)) {
            ListView_SetColumnWidth(g_app.resultsGrid, 0, pathColWidth);
            ListView_SetColumnWidth(g_app.resultsGrid, 1, colType);
            ListView_SetColumnWidth(g_app.resultsGrid, 2, pathColWidth);
            ListView_SetColumnWidth(g_app.resultsGrid, 3, colValid);
        }
    }

    int taskLabelY = bottomBarY - Theme::Scale(20);
    SetWindowPos(g_app.currentTaskLabel, nullptr, pad, taskLabelY, Theme::Scale(100), Theme::Scale(18), SWP_NOZORDER);

    int exportBtnW = Theme::Scale(110);
    int deleteBtnW = Theme::Scale(110);
    int taskEditW = contentWidth - pad * 4 - exportBtnW - deleteBtnW;
    if (taskEditW < Theme::Scale(80)) taskEditW = Theme::Scale(80);

    SetWindowPos(g_app.currentTaskEdit, nullptr, pad, bottomBarY, taskEditW, Theme::Scale(28), SWP_NOZORDER);
    SetWindowPos(g_app.exportBtn, nullptr, contentWidth - pad - deleteBtnW - pad / 2 - exportBtnW, bottomBarY - Theme::Scale(2), exportBtnW, btnH, SWP_NOZORDER);
    SetWindowPos(g_app.deleteLinkBtn, nullptr, contentWidth - pad - deleteBtnW, bottomBarY - Theme::Scale(2), deleteBtnW, btnH, SWP_NOZORDER);
}

void HandleDropFiles(HDROP hDrop) {
    wchar_t droppedPath[MAX_PATH] = {};
    if (DragQueryFileW(hDrop, 0, droppedPath, MAX_PATH)) {
        DWORD attr = GetFileAttributesW(droppedPath);
        bool isDir = (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY));

        SwitchPage(0);

        if (isDir) {
            wchar_t currentLink[MAX_PATH] = {};
            GetWindowTextW(g_app.linkFolderEdit, currentLink, MAX_PATH);
            if (wcslen(currentLink) == 0) {
                SetWindowTextW(g_app.linkFolderEdit, droppedPath);
            } else {
                SetWindowTextW(g_app.destEdit, droppedPath);
                SendMessageW(g_app.targetTypeCombo, CB_SETCURSEL, TARGET_FOLDER, 0);
            }
        } else {
            SetWindowTextW(g_app.destEdit, droppedPath);
            SendMessageW(g_app.targetTypeCombo, CB_SETCURSEL, TARGET_FILE, 0);
        }

        UpdateControlStates();
    }
    DragFinish(hDrop);
}

void UpdateControlStates() {
    int linkType = static_cast<int>(SendMessageW(g_app.typeCombo, CB_GETCURSEL, 0, 0));
    int targetKind = static_cast<int>(SendMessageW(g_app.targetTypeCombo, CB_GETCURSEL, 0, 0));

    // Hard links only work on files
    // Junctions only work on folders
    if (targetKind == TARGET_FOLDER && linkType == LINK_HARD) {
        SendMessageW(g_app.typeCombo, CB_SETCURSEL, LINK_SYMBOLIC, 0);
        linkType = LINK_SYMBOLIC;
    } else if (targetKind == TARGET_FILE && linkType == LINK_JUNCTION) {
        SendMessageW(g_app.typeCombo, CB_SETCURSEL, LINK_SYMBOLIC, 0);
        linkType = LINK_SYMBOLIC;
    }

    EnableWindow(g_app.relativeLinkCheck, linkType == LINK_SYMBOLIC);

    SetWindowTextW(g_app.linkFolderLabel, linkType == LINK_HARD ? L"Link File Location" : L"Link Folder Location");
    SetWindowTextW(g_app.destLabel, targetKind == TARGET_FOLDER ? L"Destination Folder" : L"Destination File");

    wchar_t folderBuf[MAX_PATH] = {}, destBuf[MAX_PATH] = {}, nameBuf[MAX_PATH] = {};
    GetWindowTextW(g_app.linkFolderEdit, folderBuf, MAX_PATH);
    GetWindowTextW(g_app.destEdit, destBuf, MAX_PATH);
    GetWindowTextW(g_app.linkNameEdit, nameBuf, MAX_PATH);

    bool canCreate = (wcslen(folderBuf) > 0 && wcslen(destBuf) > 0);
    EnableWindow(g_app.createLinkBtn, canCreate);
}

void PopulateGrid() {
    ListView_DeleteAllItems(g_app.resultsGrid);
    std::lock_guard<std::mutex> lock(g_app.rowsMutex);

    for (size_t i = 0; i < g_app.filteredRows.size(); ++i) {
        const auto& r = g_app.filteredRows[i];
        LVITEMW item = {};
        item.mask = LVIF_TEXT;
        item.iItem = static_cast<int>(i);
        item.pszText = const_cast<LPWSTR>(r.source.c_str());
        ListView_InsertItem(g_app.resultsGrid, &item);

        ListView_SetItemText(g_app.resultsGrid, static_cast<int>(i), 1, const_cast<LPWSTR>(r.type.c_str()));
        ListView_SetItemText(g_app.resultsGrid, static_cast<int>(i), 2, const_cast<LPWSTR>(r.target.c_str()));
        ListView_SetItemText(g_app.resultsGrid, static_cast<int>(i), 3, const_cast<LPWSTR>(r.valid ? L"Yes" : L"No (Broken)"));
    }
}

void ApplyFilter() {
    int filterIndex = static_cast<int>(SendMessageW(g_app.filterCombo, CB_GETCURSEL, 0, 0));
    std::lock_guard<std::mutex> lock(g_app.rowsMutex);

    g_app.filteredRows.clear();
    for (const auto& r : g_app.rows) {
        if (filterIndex == 1 && r.type != L"SymLink") continue;
        if (filterIndex == 2 && r.type != L"Junction") continue;
        if (filterIndex == 3 && r.type != L"Shortcut") continue;
        if (filterIndex == 4 && r.valid) continue; // Broken only
        g_app.filteredRows.push_back(r);
    }

    PopulateGrid();
}

static void CopyToClipboard(HWND hwnd, const std::wstring& text) {
    if (!OpenClipboard(hwnd)) return;
    EmptyClipboard();
    size_t bytes = (text.length() + 1) * sizeof(wchar_t);
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (hMem) {
        memcpy(GlobalLock(hMem), text.c_str(), bytes);
        GlobalUnlock(hMem);
        SetClipboardData(CF_UNICODETEXT, hMem);
    }
    CloseClipboard();
}

void ShowContextMenu(HWND hwnd, int screenX, int screenY) {
    int selIndex = ListView_GetNextItem(g_app.resultsGrid, -1, LVNI_SELECTED);
    if (selIndex < 0 || selIndex >= static_cast<int>(g_app.filteredRows.size())) return;

    LinkRow selectedRow;
    {
        std::lock_guard<std::mutex> lock(g_app.rowsMutex);
        selectedRow = g_app.filteredRows[selIndex];
    }

    HMENU hMenu = CreatePopupMenu();
    AppendMenuW(hMenu, MF_STRING, 101, L"Open Source in Explorer");
    AppendMenuW(hMenu, MF_STRING, 102, L"Open Target in Explorer");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING, 103, L"Copy Source Path");
    AppendMenuW(hMenu, MF_STRING, 104, L"Copy Target Path");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING, 105, L"Delete Link Entry...");

    int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_RIGHTBUTTON, screenX, screenY, 0, hwnd, nullptr);
    DestroyMenu(hMenu);

    if (cmd == 101) {
        std::wstring arg = L"/select,\"" + selectedRow.source + L"\"";
        ShellExecuteW(hwnd, L"open", L"explorer.exe", arg.c_str(), nullptr, SW_SHOWNORMAL);
    } else if (cmd == 102) {
        if (!selectedRow.target.empty() && selectedRow.valid) {
            std::wstring arg = L"/select,\"" + selectedRow.target + L"\"";
            ShellExecuteW(hwnd, L"open", L"explorer.exe", arg.c_str(), nullptr, SW_SHOWNORMAL);
        } else {
            MessageBoxW(hwnd, L"The link target does not exist or cannot be accessed.", L"Target Inaccessible", MB_OK | MB_ICONWARNING);
        }
    } else if (cmd == 103) {
        CopyToClipboard(hwnd, selectedRow.source);
    } else if (cmd == 104) {
        CopyToClipboard(hwnd, selectedRow.target);
    } else if (cmd == 105) {
        SendMessageW(hwnd, WM_COMMAND, MAKEWPARAM(ID_DELETE_BTN, 0), 0);
    }
}

} // namespace UiLayout
