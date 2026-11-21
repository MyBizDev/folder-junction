#include "scanner.hpp"
#include "link_ops.hpp"
#include <objbase.h>
#include <filesystem>
#include <chrono>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace Scanner {

static void ScanDirectoryRecursive(
    const fs::path& currentDir,
    int depth,
    std::vector<LinkRow>& batch,
    std::chrono::steady_clock::time_point& lastProgressTime
) {
    if (g_app.cancelScan.load() || depth > 5) return;

    std::error_code ec;
    fs::directory_iterator it(currentDir, fs::directory_options::skip_permission_denied, ec), end;
    if (ec) return;

    for (; it != end && !g_app.cancelScan.load(); it.increment(ec)) {
        if (ec) { ec.clear(); continue; }

        const auto itemPath = it->path();
        std::wstring pathStr = itemPath.wstring();

        DWORD attr = GetFileAttributesW(pathStr.c_str());
        if (attr == INVALID_FILE_ATTRIBUTES) continue;
        if (attr & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_READONLY)) continue;

        auto now = std::chrono::steady_clock::now();
        if (now - lastProgressTime >= std::chrono::milliseconds(50)) {
            lastProgressTime = now;
            auto* pMsg = new std::wstring(pathStr);
            if (!PostMessageW(g_app.hwnd, WM_SCAN_PROGRESS, 0, reinterpret_cast<LPARAM>(pMsg))) {
                delete pMsg;
            }
        }

        LinkRow row;
        row.source = pathStr;
        bool found = false;

        if (attr & FILE_ATTRIBUTE_REPARSE_POINT) {
            found = LinkOps::ReadReparseTarget(row.source, row.target, row.type, row.valid);
        } else {
            std::wstring ext = itemPath.extension().wstring();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);
            if (ext == L".lnk") {
                row.type = L"Shortcut";
                row.target = LinkOps::GetShortcutTarget(row.source);
                found = !row.target.empty();
                std::error_code targetEc;
                row.valid = found && fs::exists(fs::path(row.target), targetEc);
            }
        }

        if (found) {
            batch.push_back(std::move(row));
            if (batch.size() >= 25) {
                auto* pBatch = new std::vector<LinkRow>(std::move(batch));
                batch.clear();
                if (!PostMessageW(g_app.hwnd, WM_SCAN_BATCH, 0, reinterpret_cast<LPARAM>(pBatch))) {
                    delete pBatch;
                }
            }
        }

        if ((attr & FILE_ATTRIBUTE_DIRECTORY) && !(attr & FILE_ATTRIBUTE_REPARSE_POINT) && depth < 5) {
            ScanDirectoryRecursive(itemPath, depth + 1, batch, lastProgressTime);
        }
    }
}

void Start(const std::wstring& rootDir) {
    if (g_app.scanning.exchange(true)) {
        // Toggle action: if already scanning, request cancellation
        g_app.cancelScan = true;
        return;
    }

    if (g_app.scanWorker.joinable()) {
        g_app.scanWorker.join();
    }

    g_app.cancelScan = false;
    {
        std::lock_guard<std::mutex> lock(g_app.rowsMutex);
        g_app.rows.clear();
        g_app.filteredRows.clear();
    }

    g_app.searchRoot = rootDir;

    g_app.scanWorker = std::thread([]() {
        CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

        std::vector<LinkRow> localBatch;
        auto lastProgress = std::chrono::steady_clock::now();

        ScanDirectoryRecursive(fs::path(g_app.searchRoot), 0, localBatch, lastProgress);

        if (!localBatch.empty()) {
            auto* pBatch = new std::vector<LinkRow>(std::move(localBatch));
            localBatch.clear();
            if (!PostMessageW(g_app.hwnd, WM_SCAN_BATCH, 0, reinterpret_cast<LPARAM>(pBatch))) {
                delete pBatch;
            }
        }

        CoUninitialize();
        PostMessageW(g_app.hwnd, WM_SCAN_DONE, 0, 0);
    });
}

void Stop() {
    g_app.cancelScan = true;
    if (g_app.scanWorker.joinable()) {
        g_app.scanWorker.join();
    }
    g_app.scanning = false;
}

bool IsRunning() {
    return g_app.scanning.load();
}

static std::string EscapeCsv(const std::wstring& ws) {
    int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), static_cast<int>(ws.length()), nullptr, 0, nullptr, nullptr);
    std::string s(sizeNeeded, 0);
    WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), static_cast<int>(ws.length()), &s[0], sizeNeeded, nullptr, nullptr);

    bool needsQuotes = false;
    if (s.find_first_of(",\"\r\n") != std::string::npos) {
        needsQuotes = true;
    }

    if (!needsQuotes) return s;

    std::string escaped = "\"";
    for (char c : s) {
        if (c == '"') escaped += "\"\"";
        else escaped += c;
    }
    escaped += "\"";
    return escaped;
}

bool ExportToCsv(const std::wstring& filePath, const std::vector<LinkRow>& rows) {
    std::ofstream out(filePath, std::ios::binary);
    if (!out.is_open()) return false;

    // Write UTF-8 BOM
    const unsigned char bom[] = { 0xEF, 0xBB, 0xBF };
    out.write(reinterpret_cast<const char*>(bom), sizeof(bom));

    out << "Source,Type,Target,TargetExists\r\n";
    for (const auto& r : rows) {
        out << EscapeCsv(r.source) << ","
            << EscapeCsv(r.type) << ","
            << EscapeCsv(r.target) << ","
            << (r.valid ? "Yes" : "No") << "\r\n";
    }

    return true;
}

} // namespace Scanner
