param(
    [string]$Version = "0.1.0",
    [string]$QtPrefix = "C:\Users\Lenovo\Qt\6.8.3\msvc2022_64",
    [string]$VCRuntimeDir = "D:\Visual Studio\VS\VC\Redist\MSVC\14.50.35710\x64\Microsoft.VC145.CRT"
)

$ErrorActionPreference = "Stop"

$root = Resolve-Path (Join-Path $PSScriptRoot "..")
$buildDir = Join-Path $root "build"
$releaseDir = Join-Path $buildDir "Release"
$exe = Join-Path $releaseDir "StorageBoxLauncher.exe"
$packageName = "StorageBoxLauncher-v$Version-win64"
$distDir = Join-Path $root "dist"
$packageDir = Join-Path $distDir $packageName
$zipPath = Join-Path $distDir "$packageName.zip"

cmake -S $root -B $buildDir -DCMAKE_PREFIX_PATH=$QtPrefix
cmake --build $buildDir --config Release

& (Join-Path $QtPrefix "bin\windeployqt.exe") $exe

if (Test-Path $packageDir) {
    Remove-Item -LiteralPath $packageDir -Recurse -Force
}
New-Item -ItemType Directory -Path $packageDir | Out-Null

$exclude = @("*.ilk", "*.pdb", "*.exp", "*.lib")
Get-ChildItem -LiteralPath $releaseDir -Force | ForEach-Object {
    $skip = $false
    foreach ($pattern in $exclude) {
        if ($_.Name -like $pattern) {
            $skip = $true
            break
        }
    }
    if (-not $skip) {
        Copy-Item -LiteralPath $_.FullName -Destination $packageDir -Recurse -Force
    }
}

if (Test-Path $VCRuntimeDir) {
    Get-ChildItem -LiteralPath $VCRuntimeDir -Filter "*.dll" | ForEach-Object {
        Copy-Item -LiteralPath $_.FullName -Destination $packageDir -Force
    }
}

Copy-Item -LiteralPath (Join-Path $root "packaging\README_PORTABLE.md") -Destination (Join-Path $packageDir "README.txt") -Force

if (Test-Path $zipPath) {
    Remove-Item -LiteralPath $zipPath -Force
}
Compress-Archive -Path (Join-Path $packageDir "*") -DestinationPath $zipPath -Force

Write-Output $zipPath
