#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#include <commctrl.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <tlhelp32.h>
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <sstream>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "uuid.lib")

#define IDI_APP 1
#define IDR_PAYLOAD 101
#define IDR_LICENSE 102

#define IDC_INSTALL_DIR   1001
#define IDC_BROWSE_BTN    1002
#define IDC_START_MENU    1003
#define IDC_DESKTOP_ICON  1004
#define IDC_LAUNCH_APP    1005
#define IDC_INSTALL_BTN   1006
#define IDC_CANCEL_BTN    1007
#define IDC_STATUS_TEXT   1008

static bool CloseRunningInstances(HWND parentHwnd, bool silent) {
    std::vector<DWORD> pids;
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W pe = { sizeof(PROCESSENTRY32W) };
        if (Process32FirstW(hSnap, &pe)) {
            do {
                if (_wcsicmp(pe.szExeFile, L"FolderJunction.exe") == 0) {
                    pids.push_back(pe.th32ProcessID);
                }
            } while (Process32NextW(hSnap, &pe));
        }
        CloseHandle(hSnap);
    }

    if (pids.empty()) {
        HWND hRunning = FindWindowW(L"FolderJunctionMainWindow", nullptr);
        if (hRunning) {
            DWORD pid = 0;
            GetWindowThreadProcessId(hRunning, &pid);
            if (pid) pids.push_back(pid);
        }
    }

    if (pids.empty()) return true;

    if (!silent) {
        int ans = MessageBoxW(
            parentHwnd,
            L"Folder Junction is currently running.\n\nSetup must close it to continue with the installation.\n\nWould you like Setup to close it now?",
            L"Folder Junction is Running",
            MB_YESNO | MB_ICONQUESTION
        );
        if (ans != IDYES) {
            return false;
        }
    }

    // Attempt graceful close first
    HWND hMain = FindWindowW(L"FolderJunctionMainWindow", nullptr);
    if (hMain) {
        PostMessageW(hMain, WM_CLOSE, 0, 0);
    }

    // Wait up to 2.5 seconds for processes to terminate
    for (DWORD pid : pids) {
        HANDLE hProc = OpenProcess(SYNCHRONIZE | PROCESS_TERMINATE, FALSE, pid);
        if (hProc) {
            DWORD waitRes = WaitForSingleObject(hProc, 2500);
            if (waitRes == WAIT_TIMEOUT) {
                TerminateProcess(hProc, 0);
                WaitForSingleObject(hProc, 1000);
            }
            CloseHandle(hProc);
        }
    }

    // Allow Windows file handles to release
    Sleep(400);
    return true;
}

static bool CreateShortcut(const std::wstring& targetPath, const std::wstring& shortcutPath, const std::wstring& description, const std::wstring& workingDir) {
    IShellLinkW* psl = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&psl));
    if (SUCCEEDED(hr)) {
        psl->SetPath(targetPath.c_str());
        psl->SetDescription(description.c_str());
        psl->SetWorkingDirectory(workingDir.c_str());
        psl->SetIconLocation(targetPath.c_str(), 0);

        IPersistFile* ppf = nullptr;
        hr = psl->QueryInterface(IID_PPV_ARGS(&ppf));
        if (SUCCEEDED(hr)) {
            hr = ppf->Save(shortcutPath.c_str(), TRUE);
            ppf->Release();
        }
        psl->Release();
    }
    return SUCCEEDED(hr);
}

static bool ExtractResource(int resId, const std::wstring& outPath) {
    HMODULE hMod = GetModuleHandleW(nullptr);
    HRSRC hRes = FindResourceW(hMod, MAKEINTRESOURCEW(resId), RT_RCDATA);
    if (!hRes) return false;
    HGLOBAL hData = LoadResource(hMod, hRes);
    if (!hData) return false;
    DWORD size = SizeofResource(hMod, hRes);
    void* pData = LockResource(hData);
    if (!pData || size == 0) return false;

    HANDLE hFile = INVALID_HANDLE_VALUE;
    for (int retry = 0; retry < 6; ++retry) {
        hFile = CreateFileW(outPath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hFile != INVALID_HANDLE_VALUE) break;
        Sleep(250);
    }
    if (hFile == INVALID_HANDLE_VALUE) return false;

    DWORD written = 0;
    BOOL ok = WriteFile(hFile, pData, size, &written, nullptr);
    CloseHandle(hFile);
    return ok && (written == size);
}

static void RegisterInAddRemovePrograms(const std::wstring& installDir, const std::wstring& exePath, const std::wstring& uninstallerPath) {
    HKEY hKey = nullptr;
    const wchar_t* subKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\FolderJunction";
    if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, subKey, 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hKey, nullptr) != ERROR_SUCCESS) {
        if (RegCreateKeyExW(HKEY_CURRENT_USER, subKey, 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hKey, nullptr) != ERROR_SUCCESS) {
            return;
        }
    }

    auto setStr = [&](const wchar_t* name, const std::wstring& val) {
        RegSetValueExW(hKey, name, 0, REG_SZ, (const BYTE*)val.c_str(), (DWORD)((val.length() + 1) * sizeof(wchar_t)));
    };
    auto setDword = [&](const wchar_t* name, DWORD val) {
        RegSetValueExW(hKey, name, 0, REG_DWORD, (const BYTE*)&val, sizeof(DWORD));
    };

    setStr(L"DisplayName", L"Folder Junction");
    setStr(L"DisplayVersion", L"1.0");
    setStr(L"Publisher", L"Abhishek Chaurasia");
    setStr(L"InstallLocation", installDir);
    setStr(L"DisplayIcon", exePath + L",0");
    std::wstring uninstCmd = L"\"" + uninstallerPath + L"\" /uninstall";
    std::wstring quietUninstCmd = L"\"" + uninstallerPath + L"\" /uninstall /silent";
    setStr(L"UninstallString", uninstCmd);
    setStr(L"QuietUninstallString", quietUninstCmd);
    setDword(L"NoModify", 1);
    setDword(L"NoRepair", 1);
    setDword(L"EstimatedSize", 1024);

    RegCloseKey(hKey);
}

static void UnregisterFromAddRemovePrograms() {
    const wchar_t* subKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\FolderJunction";
    RegDeleteKeyW(HKEY_LOCAL_MACHINE, subKey);
    RegDeleteKeyW(HKEY_CURRENT_USER, subKey);
}

static bool DoUninstall(bool silent) {
    if (!silent) {
        if (MessageBoxW(nullptr, L"Are you sure you want to completely remove Folder Junction and all its components?", L"Folder Junction Uninstall", MB_YESNO | MB_ICONQUESTION) != IDYES) {
            return false;
        }
    }

    CloseRunningInstances(nullptr, silent);

    wchar_t currentExe[MAX_PATH]{};
    GetModuleFileNameW(nullptr, currentExe, MAX_PATH);
    std::wstring installDir = std::filesystem::path(currentExe).parent_path().wstring();

    wchar_t commonPrograms[MAX_PATH]{};
    if (SHGetFolderPathW(nullptr, CSIDL_COMMON_PROGRAMS, nullptr, 0, commonPrograms) == S_OK) {
        std::wstring startMenuShortcut = std::wstring(commonPrograms) + L"\\Folder Junction.lnk";
        DeleteFileW(startMenuShortcut.c_str());
    }
    wchar_t userPrograms[MAX_PATH]{};
    if (SHGetFolderPathW(nullptr, CSIDL_PROGRAMS, nullptr, 0, userPrograms) == S_OK) {
        std::wstring startMenuShortcut = std::wstring(userPrograms) + L"\\Folder Junction.lnk";
        DeleteFileW(startMenuShortcut.c_str());
    }

    wchar_t commonDesktop[MAX_PATH]{};
    if (SHGetFolderPathW(nullptr, CSIDL_COMMON_DESKTOPDIRECTORY, nullptr, 0, commonDesktop) == S_OK) {
        std::wstring desktopShortcut = std::wstring(commonDesktop) + L"\\Folder Junction.lnk";
        DeleteFileW(desktopShortcut.c_str());
    }
    wchar_t userDesktop[MAX_PATH]{};
    if (SHGetFolderPathW(nullptr, CSIDL_DESKTOPDIRECTORY, nullptr, 0, userDesktop) == S_OK) {
        std::wstring desktopShortcut = std::wstring(userDesktop) + L"\\Folder Junction.lnk";
        DeleteFileW(desktopShortcut.c_str());
    }

    UnregisterFromAddRemovePrograms();

    std::wstring exeFile = installDir + L"\\FolderJunction.exe";
    std::wstring licFile = installDir + L"\\LICENSE.txt";
    DeleteFileW(exeFile.c_str());
    DeleteFileW(licFile.c_str());

    std::wstring cmd = L"/c ping 127.0.0.1 -n 2 > nul & del /f /q \"" + std::wstring(currentExe) + L"\" & rmdir \"" + installDir + L"\"";
    ShellExecuteW(nullptr, L"open", L"cmd.exe", cmd.c_str(), nullptr, SW_HIDE);

    if (!silent) {
        MessageBoxW(nullptr, L"Folder Junction was successfully removed from your computer.", L"Folder Junction Uninstall", MB_OK | MB_ICONINFORMATION);
    }
    return true;
}

static bool DoInstall(
    HWND parentHwnd,
    const std::wstring& targetDir,
    bool createStartMenu,
    bool createDesktop,
    bool launchApp,
    bool silent,
    std::wstring& errorMsg
) {
    if (!CloseRunningInstances(parentHwnd, silent)) {
        errorMsg = L"Setup cannot continue while Folder Junction is open. Please close the app and retry.";
        return false;
    }

    std::error_code ec;
    std::filesystem::create_directories(targetDir, ec);
    if (ec) {
        errorMsg = L"Failed to create directory: " + targetDir;
        return false;
    }

    std::wstring exePath = targetDir + L"\\FolderJunction.exe";
    std::wstring licPath = targetDir + L"\\LICENSE.txt";
    std::wstring uninstPath = targetDir + L"\\Uninstall.exe";

    if (!ExtractResource(IDR_PAYLOAD, exePath)) {
        errorMsg = L"Failed to extract FolderJunction.exe to " + exePath;
        return false;
    }

    ExtractResource(IDR_LICENSE, licPath);

    wchar_t currentExe[MAX_PATH]{};
    GetModuleFileNameW(nullptr, currentExe, MAX_PATH);
    CopyFileW(currentExe, uninstPath.c_str(), FALSE);

    RegisterInAddRemovePrograms(targetDir, exePath, uninstPath);

    if (createStartMenu) {
        wchar_t commonPrograms[MAX_PATH]{};
        if (SHGetFolderPathW(nullptr, CSIDL_COMMON_PROGRAMS, nullptr, 0, commonPrograms) == S_OK) {
            std::wstring shortcutPath = std::wstring(commonPrograms) + L"\\Folder Junction.lnk";
            CreateShortcut(exePath, shortcutPath, L"Lightweight native Windows link manager", targetDir);
        }
    }

    if (createDesktop) {
        wchar_t commonDesktop[MAX_PATH]{};
        if (SHGetFolderPathW(nullptr, CSIDL_COMMON_DESKTOPDIRECTORY, nullptr, 0, commonDesktop) == S_OK) {
            std::wstring shortcutPath = std::wstring(commonDesktop) + L"\\Folder Junction.lnk";
            CreateShortcut(exePath, shortcutPath, L"Lightweight native Windows link manager", targetDir);
        }
    }

    if (launchApp) {
        ShellExecuteW(nullptr, L"open", exePath.c_str(), nullptr, targetDir.c_str(), SW_SHOWNORMAL);
    }

    return true;
}

static std::wstring GetDefaultProgramFilesDir() {
    wchar_t pf[MAX_PATH]{};
    if (SHGetFolderPathW(nullptr, CSIDL_PROGRAM_FILES, nullptr, 0, pf) == S_OK) {
        return std::wstring(pf) + L"\\Folder Junction";
    }
    return L"C:\\Program Files\\Folder Junction";
}

struct InstallerWndData {
    HWND hEditDir = nullptr;
    HWND hCheckStartMenu = nullptr;
    HWND hCheckDesktop = nullptr;
    HWND hCheckLaunch = nullptr;
    HWND hBtnInstall = nullptr;
    HWND hBtnCancel = nullptr;
    HWND hStatus = nullptr;
    HFONT hFontUI = nullptr;
    HFONT hFontTitle = nullptr;
    HFONT hFontMuted = nullptr;
};

static LRESULT CALLBACK InstallerWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    auto* data = (InstallerWndData*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);

    switch (msg) {
    case WM_CREATE: {
        auto* createStruct = (CREATESTRUCTW*)lParam;
        data = (InstallerWndData*)createStruct->lpCreateParams;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)data);

        NONCLIENTMETRICSW ncm{ sizeof(NONCLIENTMETRICSW) };
        SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
        data->hFontUI = CreateFontIndirectW(&ncm.lfMessageFont);

        LOGFONTW lfTitle = ncm.lfMessageFont;
        lfTitle.lfHeight = (LONG)(lfTitle.lfHeight * 1.35);
        lfTitle.lfWeight = FW_BOLD;
        data->hFontTitle = CreateFontIndirectW(&lfTitle);

        LOGFONTW lfMuted = ncm.lfMessageFont;
        lfMuted.lfHeight = (LONG)(lfMuted.lfHeight * 0.9);
        data->hFontMuted = CreateFontIndirectW(&lfMuted);

        HWND hTitle = CreateWindowExW(0, L"STATIC", L"Folder Junction Setup v1.0", WS_CHILD | WS_VISIBLE, 24, 20, 440, 26, hwnd, nullptr, nullptr, nullptr);
        SendMessageW(hTitle, WM_SETFONT, (WPARAM)data->hFontTitle, TRUE);

        HWND hSubtitle = CreateWindowExW(0, L"STATIC", L"Native Windows Symbolic Link, Hard Link, and Junction Manager", WS_CHILD | WS_VISIBLE, 24, 48, 440, 20, hwnd, nullptr, nullptr, nullptr);
        SendMessageW(hSubtitle, WM_SETFONT, (WPARAM)data->hFontUI, TRUE);

        HWND hDirLabel = CreateWindowExW(0, L"STATIC", L"Installation Folder:", WS_CHILD | WS_VISIBLE, 24, 84, 440, 20, hwnd, nullptr, nullptr, nullptr);
        SendMessageW(hDirLabel, WM_SETFONT, (WPARAM)data->hFontUI, TRUE);

        data->hEditDir = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", GetDefaultProgramFilesDir().c_str(), WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | WS_TABSTOP, 24, 108, 340, 26, hwnd, (HMENU)IDC_INSTALL_DIR, nullptr, nullptr);
        SendMessageW(data->hEditDir, WM_SETFONT, (WPARAM)data->hFontUI, TRUE);

        HWND hBrowse = CreateWindowExW(0, L"BUTTON", L"Browse...", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP, 374, 107, 90, 28, hwnd, (HMENU)IDC_BROWSE_BTN, nullptr, nullptr);
        SendMessageW(hBrowse, WM_SETFONT, (WPARAM)data->hFontUI, TRUE);

        data->hCheckStartMenu = CreateWindowExW(0, L"BUTTON", L"Create Start Menu shortcut", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | WS_TABSTOP, 24, 150, 300, 24, hwnd, (HMENU)IDC_START_MENU, nullptr, nullptr);
        SendMessageW(data->hCheckStartMenu, WM_SETFONT, (WPARAM)data->hFontUI, TRUE);
        SendMessageW(data->hCheckStartMenu, BM_SETCHECK, BST_CHECKED, 0);

        data->hCheckDesktop = CreateWindowExW(0, L"BUTTON", L"Create Desktop shortcut", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | WS_TABSTOP, 24, 178, 300, 24, hwnd, (HMENU)IDC_DESKTOP_ICON, nullptr, nullptr);
        SendMessageW(data->hCheckDesktop, WM_SETFONT, (WPARAM)data->hFontUI, TRUE);
        SendMessageW(data->hCheckDesktop, BM_SETCHECK, BST_CHECKED, 0);

        data->hCheckLaunch = CreateWindowExW(0, L"BUTTON", L"Launch Folder Junction after installation", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | WS_TABSTOP, 24, 206, 350, 24, hwnd, (HMENU)IDC_LAUNCH_APP, nullptr, nullptr);
        SendMessageW(data->hCheckLaunch, WM_SETFONT, (WPARAM)data->hFontUI, TRUE);
        SendMessageW(data->hCheckLaunch, BM_SETCHECK, BST_CHECKED, 0);

        data->hStatus = CreateWindowExW(0, L"STATIC", L"Ready to install.", WS_CHILD | WS_VISIBLE, 24, 244, 440, 20, hwnd, (HMENU)IDC_STATUS_TEXT, nullptr, nullptr);
        SendMessageW(data->hStatus, WM_SETFONT, (WPARAM)data->hFontUI, TRUE);

        // Publisher note at bottom
        HWND hPublisher = CreateWindowExW(0, L"STATIC", L"Publisher: Abhishek Chaurasia", WS_CHILD | WS_VISIBLE, 24, 288, 220, 24, hwnd, nullptr, nullptr, nullptr);
        SendMessageW(hPublisher, WM_SETFONT, (WPARAM)data->hFontMuted, TRUE);

        data->hBtnInstall = CreateWindowExW(0, L"BUTTON", L"Install", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON | WS_TABSTOP, 268, 280, 96, 32, hwnd, (HMENU)IDC_INSTALL_BTN, nullptr, nullptr);
        SendMessageW(data->hBtnInstall, WM_SETFONT, (WPARAM)data->hFontUI, TRUE);

        data->hBtnCancel = CreateWindowExW(0, L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP, 374, 280, 90, 32, hwnd, (HMENU)IDC_CANCEL_BTN, nullptr, nullptr);
        SendMessageW(data->hBtnCancel, WM_SETFONT, (WPARAM)data->hFontUI, TRUE);

        return 0;
    }

    case WM_COMMAND: {
        int id = LOWORD(wParam);
        if (id == IDC_BROWSE_BTN) {
            IFileOpenDialog* dlg = nullptr;
            if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dlg)))) {
                dlg->SetOptions(FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
                dlg->SetTitle(L"Select Installation Folder");
                if (SUCCEEDED(dlg->Show(hwnd))) {
                    IShellItem* res = nullptr;
                    if (SUCCEEDED(dlg->GetResult(&res))) {
                        PWSTR path = nullptr;
                        if (SUCCEEDED(res->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
                            std::wstring chosen = path;
                            if (chosen.find(L"Folder Junction") == std::wstring::npos) {
                                chosen += L"\\Folder Junction";
                            }
                            SetWindowTextW(data->hEditDir, chosen.c_str());
                            CoTaskMemFree(path);
                        }
                        res->Release();
                    }
                }
                dlg->Release();
            }
        }
        else if (id == IDC_INSTALL_BTN) {
            wchar_t dirBuf[MAX_PATH]{};
            GetWindowTextW(data->hEditDir, dirBuf, MAX_PATH);
            std::wstring installDir = dirBuf;
            if (installDir.empty()) {
                MessageBoxW(hwnd, L"Please enter a valid installation directory.", L"Folder Junction Setup", MB_OK | MB_ICONWARNING);
                return 0;
            }

            bool startMenu = (SendMessageW(data->hCheckStartMenu, BM_GETCHECK, 0, 0) == BST_CHECKED);
            bool desktop = (SendMessageW(data->hCheckDesktop, BM_GETCHECK, 0, 0) == BST_CHECKED);
            bool launch = (SendMessageW(data->hCheckLaunch, BM_GETCHECK, 0, 0) == BST_CHECKED);

            EnableWindow(data->hBtnInstall, FALSE);
            SetWindowTextW(data->hStatus, L"Installing Folder Junction...");

            std::wstring err;
            if (!DoInstall(hwnd, installDir, startMenu, desktop, launch, false, err)) {
                MessageBoxW(hwnd, (L"Installation failed: " + err).c_str(), L"Folder Junction Setup", MB_OK | MB_ICONERROR);
                EnableWindow(data->hBtnInstall, TRUE);
                SetWindowTextW(data->hStatus, L"Installation failed.");
            } else {
                SetWindowTextW(data->hStatus, L"Installation completed successfully!");
                MessageBoxW(hwnd, L"Folder Junction v1.0 has been successfully installed!", L"Folder Junction Setup", MB_OK | MB_ICONINFORMATION);
                DestroyWindow(hwnd);
            }
        }
        else if (id == IDC_CANCEL_BTN) {
            DestroyWindow(hwnd);
        }
        return 0;
    }

    case WM_DESTROY:
        if (data) {
            if (data->hFontUI) DeleteObject(data->hFontUI);
            if (data->hFontTitle) DeleteObject(data->hFontTitle);
            if (data->hFontMuted) DeleteObject(data->hFontMuted);
        }
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR lpCmdLine, int nCmdShow) {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    std::wstring cmd = lpCmdLine ? lpCmdLine : L"";
    std::wstring lowerCmd;
    for (wchar_t c : cmd) lowerCmd += (wchar_t)towlower(c);

    bool isUninstall = (lowerCmd.find(L"/uninstall") != std::wstring::npos || lowerCmd.find(L"-uninstall") != std::wstring::npos);
    bool isSilent = (lowerCmd.find(L"/silent") != std::wstring::npos || lowerCmd.find(L"/s") != std::wstring::npos || lowerCmd.find(L"-s") != std::wstring::npos);

    if (isUninstall) {
        DoUninstall(isSilent);
        CoUninitialize();
        return 0;
    }

    if (isSilent) {
        std::wstring err;
        DoInstall(nullptr, GetDefaultProgramFilesDir(), true, true, false, true, err);
        CoUninitialize();
        return 0;
    }

    INITCOMMONCONTROLSEX icc{ sizeof(INITCOMMONCONTROLSEX), ICC_STANDARD_CLASSES | ICC_WIN95_CLASSES };
    InitCommonControlsEx(&icc);

    HICON hIconBig = (HICON)LoadImageW(hInstance, MAKEINTRESOURCEW(IDI_APP), IMAGE_ICON, GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), LR_DEFAULTCOLOR);
    HICON hIconSmall = (HICON)LoadImageW(hInstance, MAKEINTRESOURCEW(IDI_APP), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR);

    WNDCLASSEXW wc{ sizeof(WNDCLASSEXW) };
    wc.lpfnWndProc = InstallerWndProc;
    wc.hInstance = hInstance;
    wc.hIcon = hIconBig ? hIconBig : LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_APP));
    wc.hIconSm = hIconSmall ? hIconSmall : wc.hIcon;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = L"FolderJunctionInstaller";
    RegisterClassExW(&wc);

    InstallerWndData data;

    int winWidth = 500;
    int winHeight = 365;
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    int posX = (screenWidth - winWidth) / 2;
    int posY = (screenHeight - winHeight) / 2;

    HWND hwnd = CreateWindowExW(
        0,
        L"FolderJunctionInstaller",
        L"Folder Junction Setup v1.0",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        posX, posY, winWidth, winHeight,
        nullptr, nullptr, hInstance, &data
    );

    if (!hwnd) {
        CoUninitialize();
        return 1;
    }

    if (hIconBig) SendMessageW(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIconBig);
    if (hIconSmall) SendMessageW(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIconSmall);

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    CoUninitialize();
    return (int)msg.wParam;
}
