#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <string>

namespace LinkOps {

bool IsAdmin();
bool IsDeveloperMode();
std::wstring FormatError(DWORD errorCode);
bool IsSameVolume(const std::wstring& pathA, const std::wstring& pathB);

bool CreateSymlink(
    const std::wstring& linkPath,
    const std::wstring& targetPath,
    bool isDirectory,
    bool isRelative,
    std::wstring& outError
);

bool CreateHardLink(
    const std::wstring& linkPath,
    const std::wstring& targetPath,
    std::wstring& outError
);

bool CreateJunction(
    const std::wstring& linkPath,
    const std::wstring& targetPath,
    std::wstring& outError
);

bool ReadReparseTarget(
    const std::wstring& path,
    std::wstring& outTarget,
    std::wstring& outType,
    bool& outValid
);

std::wstring GetShortcutTarget(const std::wstring& path);

bool SafeDeleteLink(const std::wstring& path, std::wstring& outError);

} // namespace LinkOps
