# Build and validation status

This is the native C++20/Win32 source conversion package. The source archive was inspected and the new project structure, icon resource references, manifest XML, and build configuration were checked.

**The Windows executable has not yet been compiled or runtime-tested.** The available execution environment is Linux and does not contain the Windows SDK, MSVC, or a MinGW Windows cross-compiler. A successful Windows build is therefore not claimed.

## Required validation on Windows 10 and Windows 11 x64

1. Build with `build-release.ps1` using Visual Studio 2022 C++ Build Tools and the Windows 10 SDK.
2. Confirm `FolderJunction.exe` launches on a clean machine without .NET or the Visual C++ Redistributable installed.
3. Test symbolic file and folder links both with Developer Mode enabled and disabled, confirming the preserved administrator UAC prompt.
4. Test hard links (same-volume restriction), junction creation, dangling links, UNC paths, and invalid paths.
5. Scan a directory containing file symlinks, directory symlinks, junctions, `.lnk` shortcuts, inaccessible folders, and nested directories; test cancellation.
6. Confirm deletion removes only the selected link/shortcut entry and never its target.
7. Compare both screens against the original application at 100%, 125%, and 150% display scaling.

The included GitHub Actions workflow builds the x64 executable on `windows-2022` and publishes it as a workflow artifact.
