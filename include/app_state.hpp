#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <commctrl.h>
#include <string>
#include <vector>
#include <atomic>
#include <thread>
#include <mutex>

constexpr UINT WM_SCAN_BATCH    = WM_APP + 10;
constexpr UINT WM_SCAN_DONE     = WM_APP + 11;
constexpr UINT WM_SCAN_PROGRESS = WM_APP + 12;

enum ControlId : int {
    ID_NAV_CREATE        = 2,
    ID_NAV_VIEW          = 3,
    ID_CREATE_TYPE_LABEL = 10,
    ID_CREATE_TYPE       = 11,
    ID_TARGET_TYPE_LABEL = 12,
    ID_TARGET_TYPE       = 13,
    ID_GUIDE_BTN         = 14,
    ID_LINK_FOLDER_LABEL = 15,
    ID_LINK_FOLDER_EDIT  = 16,
    ID_LINK_FOLDER_BTN   = 17,
    ID_DEST_LABEL        = 18,
    ID_DEST_EDIT         = 19,
    ID_DEST_BTN          = 20,
    ID_LINK_NAME_EDIT    = 21,
    ID_USE_NAME_BTN      = 22,
    ID_INFO_LABEL        = 23,
    ID_CREATE_LINK_BTN   = 24,
    ID_REL_LINK_CHECK    = 25,
    ID_SEARCH_PATH_LABEL = 30,
    ID_SEARCH_PATH_EDIT  = 31,
    ID_SEARCH_BROWSE_BTN = 32,
    ID_SEARCH_ACTION_BTN = 33,
    ID_RESULTS_GRID      = 34,
    ID_TASK_LABEL        = 35,
    ID_TASK_EDIT         = 36,
    ID_DELETE_BTN        = 37,
    ID_EXPORT_BTN        = 38,
    ID_FILTER_TYPE       = 39,
    ID_APP_TITLE         = 40,
    ID_PUBLISHER_LABEL   = 41
};

enum LinkType : int {
    LINK_SYMBOLIC = 0,
    LINK_HARD     = 1,
    LINK_JUNCTION = 2
};

enum TargetKind : int {
    TARGET_FOLDER = 0,
    TARGET_FILE   = 1
};

struct LinkRow {
    std::wstring source;
    std::wstring type;
    std::wstring target;
    bool valid = false;
};

struct AppState {
    HWND hwnd = nullptr;
    HWND navCreate = nullptr;
    HWND navView = nullptr;
    HWND publisherLabel = nullptr;
    HWND tooltip = nullptr;
    HWND createPage = nullptr;
    HWND viewPage = nullptr;

    // Create page controls
    HWND typeCombo = nullptr;
    HWND targetTypeCombo = nullptr;
    HWND linkFolderLabel = nullptr;
    HWND linkFolderEdit = nullptr;
    HWND linkFolderBrowseBtn = nullptr;
    HWND destLabel = nullptr;
    HWND destEdit = nullptr;
    HWND destBrowseBtn = nullptr;
    HWND linkNameEdit = nullptr;
    HWND useDestNameBtn = nullptr;
    HWND relativeLinkCheck = nullptr;
    HWND createLinkBtn = nullptr;
    HWND guideBtn = nullptr;
    HWND infoStatus = nullptr;

    // View page controls
    HWND searchPathLabel = nullptr;
    HWND searchPathEdit = nullptr;
    HWND searchBrowseBtn = nullptr;
    HWND searchActionBtn = nullptr;
    HWND filterCombo = nullptr;
    HWND resultsGrid = nullptr;
    HWND currentTaskLabel = nullptr;
    HWND currentTaskEdit = nullptr;
    HWND deleteLinkBtn = nullptr;
    HWND exportBtn = nullptr;

    // UI & Graphics resources
    HFONT fontNormal = nullptr;
    HFONT fontTitle = nullptr;
    HFONT fontStatus = nullptr;
    HBRUSH brushBackground = nullptr;
    HBRUSH brushPanel = nullptr;
    HBRUSH brushField = nullptr;

    int dpi = 96;
    int activePage = 0; // 0 = Create, 1 = View
    bool guideVisible = false;

    // Scanning state
    std::atomic_bool scanning{false};
    std::atomic_bool cancelScan{false};
    std::thread scanWorker;
    std::wstring searchRoot = L"C:\\Users\\";
    std::vector<LinkRow> rows;
    std::vector<LinkRow> filteredRows;
    std::mutex rowsMutex;
};

extern AppState g_app;
