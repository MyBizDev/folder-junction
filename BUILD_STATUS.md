# Build and validation status

The native C++20 / Win32 standalone application and release installer (v1.0) have been compiled with Visual Studio MSVC on Windows 11 x64.

**Publisher:** Abhishek Chaurasia

## Build Artefacts

- **Portable Application**: `bin/FolderJunction.exe` (or `build/Release/FolderJunction.exe`)
- **Release Setup Installer**: `bin/FolderJunction-Setup.exe` (or `build/Release/FolderJunction-Setup.exe`)
- **Inno Setup Script**: `installer/FolderJunction.iss`

## Build Verification

1. Built using Visual Studio MSVC 19.51 and Windows 10/11 SDK 10.0.26100 via `build-release.ps1`.
2. Verified static MSVC runtime (`/MT`) linkage: no VC++ Redistributable or .NET dependencies required.
3. Automatically detects running instances of `FolderJunction.exe` during setup, prompting the user and closing them cleanly to prevent locked-file extraction errors.
4. Embedded PerMonitorV2 DPI awareness, long path awareness, dark mode theming, rich tooltips, drag & drop, and native title bar icons.
5. Setup installer registers under Windows Settings (Installed Apps) with publisher **Abhishek Chaurasia** and version **1.0**.
