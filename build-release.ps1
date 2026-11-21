$ErrorActionPreference = 'Stop'
$buildDir = "$PSScriptRoot/build"

# Check if Visual Studio generator is used or if default/Ninja should be used
if (Get-Command cl.exe -ErrorAction SilentlyContinue -or (Get-Command cl -ErrorAction SilentlyContinue)) {
    cmake -S $PSScriptRoot -B $buildDir -A x64 -DCMAKE_BUILD_TYPE=Release
} else {
    cmake -S $PSScriptRoot -B $buildDir -DCMAKE_BUILD_TYPE=Release
}

Write-Host "Building Release executables (FolderJunction and FolderJunction-Setup)..."
cmake --build $buildDir --config Release

$targetExe = if (Test-Path "$buildDir/Release/FolderJunction.exe") {
    "$buildDir/Release/FolderJunction.exe"
} else {
    "$buildDir/FolderJunction.exe"
}

$installerExe = if (Test-Path "$buildDir/Release/FolderJunction-Setup.exe") {
    "$buildDir/Release/FolderJunction-Setup.exe"
} else {
    "$buildDir/FolderJunction-Setup.exe"
}

# Also compile with Inno Setup if ISCC.exe is available
$isccPaths = @(
    (Get-Command iscc.exe -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Source -ErrorAction SilentlyContinue),
    "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe",
    "${env:ProgramFiles}\Inno Setup 6\ISCC.exe"
) | Where-Object { $_ -and (Test-Path $_) }

if ($isccPaths.Count -gt 0) {
    Write-Host "Compiling Inno Setup installer..."
    & $isccPaths[0] "$PSScriptRoot/installer/FolderJunction.iss"
}

Write-Host "`nBuild complete!"
Write-Host "Application: $targetExe"
Write-Host "Installer:   $installerExe"
if (Test-Path "$PSScriptRoot/bin/FolderJunction-Setup.exe") {
    Write-Host "Binaries also staged in: $PSScriptRoot\bin\"
}
