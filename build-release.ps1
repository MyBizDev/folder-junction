$ErrorActionPreference = 'Stop'
$buildDir = "$PSScriptRoot/build"

# Check if Visual Studio generator is used or if default/Ninja should be used
if (Get-Command cl.exe -ErrorAction SilentlyContinue) {
    cmake -S $PSScriptRoot -B $buildDir -A x64 -DCMAKE_BUILD_TYPE=Release
} else {
    cmake -S $PSScriptRoot -B $buildDir -DCMAKE_BUILD_TYPE=Release
}

cmake --build $buildDir --config Release --parallel

$targetExe = if (Test-Path "$buildDir/Release/FolderJunction.exe") {
    "$buildDir/Release/FolderJunction.exe"
} else {
    "$buildDir/FolderJunction.exe"
}

Write-Host "Built: $targetExe"
