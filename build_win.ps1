$ErrorActionPreference = "Stop"
Set-Location -LiteralPath $PSScriptRoot

$cmake = Get-Command cmake -ErrorAction SilentlyContinue
if (-not $cmake) {
    $fallback = "C:\Program Files\CMake\bin\cmake.exe"
    if (Test-Path $fallback) {
        $cmakePath = $fallback
    } else {
        throw "cmake not found. Install Kitware.CMake first."
    }
} else {
    $cmakePath = $cmake.Source
}

& $cmakePath --preset windows-msvc
& $cmakePath --build --preset windows-msvc-release
