# Folder Junction — Native Windows Edition

A lightweight native C++20 / Win32 application for creating and inspecting Windows symbolic links, hard links, and junctions. This is a native rewrite of [Benisgo/Symbolic11](https://github.com/Benisgo/Symbolic11), retaining the original project's GPL-3.0 licensing and icon.

## Goals

- Windows 10 and Windows 11, x64.
- One portable `FolderJunction.exe`.
- No .NET, Java, WebView2, installer, or separately installed GUI runtime.
- Static MSVC runtime (`/MT`) so the Visual C++ Redistributable is not required.
- Retain the original Create and View workflows, icon, and general dark visual design; use the Windows-installed Segoe UI Variable font where available, with system font fallback on Windows 10.
- Use Windows APIs directly; no `cmd.exe`, PowerShell, or shell command construction.

Windows system DLLs remain required. They are part of Windows and are not separately installed application dependencies.

## Features

- Create file/folder symbolic links, file hard links and folder junctions.
- Use unprivileged symbolic-link creation on Windows versions that support it; otherwise Windows permissions / Developer Mode policy applies.
- Browse source and destination paths; copy the destination's base name into the link name.
- Recursively discover symbolic links, junctions and `.lnk` shortcuts (maximum depth 5; hidden/system/read-only entries skipped, matching the original search policy).
- Show source, type, target and whether the target currently exists.
- Cancel a running scan, open a selected item in Explorer, and delete the link object without deleting its target.

## Build (Windows 10/11 x64)

Requirements for building only: Visual Studio 2022 Build Tools or Visual Studio with the **Desktop development with C++** workload, Windows 10 SDK, and CMake 3.24+.

```powershell
.\build-release.ps1
```

Or build manually:

```powershell
cmake -S . -B build -A x64 -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Output: `build/Release/FolderJunction.exe`.

The build is configured for the static MSVC runtime. No runtime installer is required on the target machine.

## Important notes

- The original application requests administrator privileges at startup; this port preserves that UAC behaviour. Developer Mode support is retained as a compatibility path for symbolic-link creation.
- Hard links must remain on the same volume and can only target files.
- Junctions are folder-only.
- Deletion is guarded by a confirmation prompt and removes the selected link/shortcut entry, not its target.
- A link can be dangling; the View screen reports target existence separately.

## Licensing and attribution

This derivative is based on [Benisgo/Symbolic11](https://github.com/Benisgo/Symbolic11), originally distributed under GPL-3.0. The GPL-3.0 license and required attribution are retained in `LICENSE.txt`. The original application icon is retained. The bundled Microsoft Segoe UI Variable font is intentionally not redistributed; the app uses the font installed with Windows to avoid introducing a font-redistribution licensing risk.

## Build status

The source port has not yet been compiled or runtime-tested on Windows. See [`BUILD_STATUS.md`](BUILD_STATUS.md) for the required validation checklist. GitHub Actions are intentionally not configured or used; build locally with the instructions above.
