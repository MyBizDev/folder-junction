#pragma once
#include "app_state.hpp"
#include <shellapi.h>

namespace UiLayout {

void CreateControls(HWND parent);
void SwitchPage(int pageIndex);
void Resize(int clientWidth, int clientHeight);
void HandleDropFiles(HDROP hDrop);
void ShowContextMenu(HWND hwnd, int screenX, int screenY);
void UpdateControlStates();
void PopulateGrid();
void ApplyFilter();

} // namespace UiLayout
