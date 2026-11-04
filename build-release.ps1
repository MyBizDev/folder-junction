$ErrorActionPreference = 'Stop'
cmake -S $PSScriptRoot -B "$PSScriptRoot/build" -A x64 -DCMAKE_BUILD_TYPE=Release
cmake --build "$PSScriptRoot/build" --config Release --parallel
Write-Host "Built: $PSScriptRoot/build/Release/FolderJunction.exe"
