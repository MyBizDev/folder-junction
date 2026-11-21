#pragma once
#include "app_state.hpp"

namespace Scanner {

void Start(const std::wstring& rootDir);
void Stop();
bool IsRunning();
bool ExportToCsv(const std::wstring& filePath, const std::vector<LinkRow>& rows);

} // namespace Scanner
