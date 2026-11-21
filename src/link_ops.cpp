#include "link_ops.hpp"
#include <winioctl.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <filesystem>
#include <vector>
#include <algorithm>

namespace fs = std::filesystem;

#ifndef SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE
#define SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE 0x2
#endif
#ifndef SYMLINK_FLAG_RELATIVE
#define SYMLINK_FLAG_RELATIVE 1
#endif

struct REPARSE_DATA_BUFFER_INTERNAL {
    ULONG ReparseTag;
    USHORT ReparseDataLength;
    USHORT Reserved;
    union {
        struct {
            USHORT SubstituteNameOffset;
            USHORT SubstituteNameLength;
            USHORT PrintNameOffset;
            USHORT PrintNameLength;
            ULONG Flags;
            WCHAR PathBuffer[1];
        } SymbolicLinkReparseBuffer;
        struct {
            USHORT SubstituteNameOffset;
            USHORT SubstituteNameLength;
            USHORT PrintNameOffset;
            USHORT PrintNameLength;
            WCHAR PathBuffer[1];
        } MountPointReparseBuffer;
    };
};

namespace LinkOps {

bool IsAdmin() {
    BOOL isAdmin = FALSE;
    PSID adminGroup = nullptr;
    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
    if (AllocateAndInitializeSid(&ntAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS,
                                  0, 0, 0, 0, 0, 0, &adminGroup)) {
        CheckTokenMembership(nullptr, adminGroup, &isAdmin);
        FreeSid(adminGroup);
    }
    return isAdmin != FALSE;
}

bool IsDeveloperMode() {
    HKEY hKey = nullptr;
    DWORD value = 0, size = sizeof(value);
    bool enabled = false;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\AppModelUnlock", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        if (RegQueryValueExW(hKey, L"AllowDevelopmentWithoutDevLicense", nullptr, nullptr, (BYTE*)&value, &size) == ERROR_SUCCESS) {
            enabled = (value == 1);
        }
        RegCloseKey(hKey);
    }
    return enabled;
}

std::wstring FormatError(DWORD errorCode) {
    if (errorCode == 0) return L"Success";
    wchar_t* buffer = nullptr;
    FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, errorCode, 0, (LPWSTR)&buffer, 0, nullptr
    );
    std::wstring result = buffer ? buffer : (L"Error " + std::to_wstring(errorCode));
    if (buffer) LocalFree(buffer);
    while (!result.empty() && (result.back() == L'\r' || result.back() == L'\n' || result.back() == L' ')) {
        result.pop_back();
    }
    return result;
}

bool IsSameVolume(const std::wstring& pathA, const std::wstring& pathB) {
    wchar_t rootA[MAX_PATH] = {}, rootB[MAX_PATH] = {};
    if (GetVolumePathNameW(pathA.c_str(), rootA, MAX_PATH) &&
        GetVolumePathNameW(pathB.c_str(), rootB, MAX_PATH)) {
        return _wcsicmp(rootA, rootB) == 0;
    }
    return false;
}

bool CreateSymlink(
    const std::wstring& linkPath,
    const std::wstring& targetPath,
    bool isDirectory,
    bool isRelative,
    std::wstring& outError
) {
    std::wstring effectiveTarget = targetPath;
    if (isRelative) {
        std::error_code ec;
        fs::path linkDir = fs::path(linkPath).parent_path();
        fs::path rel = fs::relative(fs::path(targetPath), linkDir, ec);
        if (!ec && !rel.empty()) {
            effectiveTarget = rel.wstring();
        }
    }

    DWORD flags = isDirectory ? SYMBOLIC_LINK_FLAG_DIRECTORY : 0;
#ifdef SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE
    flags |= SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE;
#endif

    BOOL ok = CreateSymbolicLinkW(linkPath.c_str(), effectiveTarget.c_str(), flags);
    if (!ok && (flags & SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE) && GetLastError() == ERROR_INVALID_PARAMETER) {
        flags = isDirectory ? SYMBOLIC_LINK_FLAG_DIRECTORY : 0;
        ok = CreateSymbolicLinkW(linkPath.c_str(), effectiveTarget.c_str(), flags);
    }

    if (!ok) {
        outError = FormatError(GetLastError());
        return false;
    }
    return true;
}

bool CreateHardLink(
    const std::wstring& linkPath,
    const std::wstring& targetPath,
    std::wstring& outError
) {
    if (!IsSameVolume(linkPath, targetPath)) {
        outError = L"Hard links must be created on the same NTFS volume as the target.";
        return false;
    }

    DWORD attr = GetFileAttributesW(targetPath.c_str());
    if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY)) {
        outError = L"Hard links can only target files, not directories.";
        return false;
    }

    if (!::CreateHardLinkW(linkPath.c_str(), targetPath.c_str(), nullptr)) {
        outError = FormatError(GetLastError());
        return false;
    }
    return true;
}

bool CreateJunction(
    const std::wstring& linkPath,
    const std::wstring& targetPath,
    std::wstring& outError
) {
    std::error_code ec;
    fs::path absTargetPath = fs::absolute(fs::path(targetPath), ec);
    if (ec) {
        outError = L"Invalid target path.";
        return false;
    }

    std::wstring absTarget = absTargetPath.wstring();
    if (absTarget.rfind(L"\\\\", 0) == 0) {
        outError = L"Junctions cannot target UNC or network share paths.";
        return false;
    }

    if (!CreateDirectoryW(linkPath.c_str(), nullptr)) {
        outError = FormatError(GetLastError());
        return false;
    }

    HANDLE hDir = CreateFileW(
        linkPath.c_str(), GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
        FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, nullptr
    );
    if (hDir == INVALID_HANDLE_VALUE) {
        outError = FormatError(GetLastError());
        RemoveDirectoryW(linkPath.c_str());
        return false;
    }

    std::wstring substitute = L"\\??\\" + absTarget;
    std::wstring print = absTarget;

    size_t subBytes = substitute.size() * sizeof(wchar_t);
    size_t printBytes = print.size() * sizeof(wchar_t);
    size_t dataLen = 8 + subBytes + sizeof(wchar_t) + printBytes + sizeof(wchar_t);

    std::vector<BYTE> buffer(8 + dataLen, 0);
    auto* r = reinterpret_cast<REPARSE_DATA_BUFFER_INTERNAL*>(buffer.data());
    r->ReparseTag = IO_REPARSE_TAG_MOUNT_POINT;
    r->Reserved = 0;
    r->ReparseDataLength = static_cast<USHORT>(dataLen - 8);

    r->MountPointReparseBuffer.SubstituteNameOffset = 0;
    r->MountPointReparseBuffer.SubstituteNameLength = static_cast<USHORT>(subBytes);
    r->MountPointReparseBuffer.PrintNameOffset = static_cast<USHORT>(subBytes + sizeof(wchar_t));
    r->MountPointReparseBuffer.PrintNameLength = static_cast<USHORT>(printBytes);

    memcpy(r->MountPointReparseBuffer.PathBuffer, substitute.c_str(), subBytes + sizeof(wchar_t));
    memcpy(
        reinterpret_cast<BYTE*>(r->MountPointReparseBuffer.PathBuffer) + subBytes + sizeof(wchar_t),
        print.c_str(),
        printBytes + sizeof(wchar_t)
    );

    DWORD returned = 0;
    BOOL ok = DeviceIoControl(hDir, FSCTL_SET_REPARSE_POINT, r, static_cast<DWORD>(buffer.size()), nullptr, 0, &returned, nullptr);
    if (!ok) {
        outError = FormatError(GetLastError());
    }

    CloseHandle(hDir);
    if (!ok) {
        RemoveDirectoryW(linkPath.c_str());
        return false;
    }
    return true;
}

bool ReadReparseTarget(
    const std::wstring& path,
    std::wstring& outTarget,
    std::wstring& outType,
    bool& outValid
) {
    HANDLE h = CreateFileW(
        path.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, nullptr
    );
    if (h == INVALID_HANDLE_VALUE) return false;

    std::vector<BYTE> buffer(MAXIMUM_REPARSE_DATA_BUFFER_SIZE);
    DWORD bytesReturned = 0;
    BOOL ok = DeviceIoControl(h, FSCTL_GET_REPARSE_POINT, nullptr, 0, buffer.data(), static_cast<DWORD>(buffer.size()), &bytesReturned, nullptr);
    CloseHandle(h);

    if (!ok || bytesReturned < 8) return false;

    auto* r = reinterpret_cast<REPARSE_DATA_BUFFER_INTERNAL*>(buffer.data());

    if (r->ReparseTag == IO_REPARSE_TAG_SYMLINK) {
        if (bytesReturned < 20) return false;
        outType = L"SymLink";
        auto& q = r->SymbolicLinkReparseBuffer;
        DWORD pathBytes = bytesReturned - 20;

        if ((q.SubstituteNameOffset | q.SubstituteNameLength) & 1) return false;
        if (static_cast<DWORD>(q.SubstituteNameOffset) + q.SubstituteNameLength > pathBytes) return false;

        outTarget.assign(q.PathBuffer + q.SubstituteNameOffset / 2, q.SubstituteNameLength / 2);
        if (outTarget.rfind(L"\\??\\UNC\\", 0) == 0) {
            outTarget = L"\\\\" + outTarget.substr(8);
        } else if (outTarget.rfind(L"\\??\\", 0) == 0) {
            outTarget.erase(0, 4);
        }

        if (q.Flags & SYMLINK_FLAG_RELATIVE) {
            std::error_code ec;
            outTarget = (fs::path(path).parent_path() / outTarget).lexically_normal().wstring();
        }
    } else if (r->ReparseTag == IO_REPARSE_TAG_MOUNT_POINT) {
        if (bytesReturned < 16) return false;
        outType = L"Junction";
        auto& q = r->MountPointReparseBuffer;
        DWORD pathBytes = bytesReturned - 16;

        if ((q.SubstituteNameOffset | q.SubstituteNameLength) & 1) return false;
        if (static_cast<DWORD>(q.SubstituteNameOffset) + q.SubstituteNameLength > pathBytes) return false;

        outTarget.assign(q.PathBuffer + q.SubstituteNameOffset / 2, q.SubstituteNameLength / 2);
        if (outTarget.rfind(L"\\??\\UNC\\", 0) == 0) {
            outTarget = L"\\\\" + outTarget.substr(8);
        } else if (outTarget.rfind(L"\\??\\", 0) == 0) {
            outTarget.erase(0, 4);
        }
    } else {
        outType = L"Reparse";
        outTarget = L"(unsupported reparse point)";
    }

    std::error_code ec;
    outValid = fs::exists(fs::path(outTarget), ec);
    return true;
}

std::wstring GetShortcutTarget(const std::wstring& path) {
    IShellLinkW* pLink = nullptr;
    std::wstring result;
    if (SUCCEEDED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pLink)))) {
        IPersistFile* pPf = nullptr;
        if (SUCCEEDED(pLink->QueryInterface(IID_PPV_ARGS(&pPf)))) {
            if (SUCCEEDED(pPf->Load(path.c_str(), STGM_READ))) {
                wchar_t targetBuf[32768] = {};
                WIN32_FIND_DATAW fd = {};
                if (SUCCEEDED(pLink->GetPath(targetBuf, static_cast<int>(std::size(targetBuf)), &fd, SLGP_RAWPATH))) {
                    result = targetBuf;
                }
            }
            pPf->Release();
        }
        pLink->Release();
    }
    return result;
}

bool SafeDeleteLink(const std::wstring& path, std::wstring& outError) {
    DWORD attr = GetFileAttributesW(path.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES) {
        outError = FormatError(GetLastError());
        return false;
    }

    BOOL ok = FALSE;
    if (attr & FILE_ATTRIBUTE_DIRECTORY) {
        ok = RemoveDirectoryW(path.c_str());
    } else {
        ok = DeleteFileW(path.c_str());
    }

    if (!ok) {
        outError = FormatError(GetLastError());
        return false;
    }
    return true;
}

} // namespace LinkOps
