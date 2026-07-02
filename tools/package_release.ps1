param(
    [string]$Version = "0.1.1",
    [string]$QtPrefix = "",
    [string]$VCRuntimeDir = "",
    [switch]$IncludeTranslations,
    [switch]$IncludeSoftwareOpenGL,
    [switch]$KeepNetworkRuntime,
    [switch]$Sign,
    [string]$CertificateThumbprint = $env:STORAGEBOX_SIGN_CERT_THUMBPRINT,
    [string]$TimestampServer = "http://timestamp.digicert.com",
    [string]$SignToolPath = $env:SIGNTOOL_EXE
)

$ErrorActionPreference = "Stop"

function Resolve-ConfiguredPath {
    param(
        [string]$Value,
        [string]$EnvironmentName,
        [string]$Fallback
    )

    if (-not [string]::IsNullOrWhiteSpace($Value)) {
        return $Value
    }

    $environmentValue = [Environment]::GetEnvironmentVariable($EnvironmentName)
    if (-not [string]::IsNullOrWhiteSpace($environmentValue)) {
        return $environmentValue
    }

    return $Fallback
}

function Invoke-Native {
    param(
        [string]$FilePath,
        [string[]]$Arguments
    )

    & $FilePath @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "$FilePath failed with exit code $LASTEXITCODE"
    }
}

function Find-SignTool {
    if (-not [string]::IsNullOrWhiteSpace($SignToolPath)) {
        if (Test-Path $SignToolPath) {
            return (Resolve-Path $SignToolPath).Path
        }
        throw "SignToolPath does not exist: $SignToolPath"
    }

    $programFilesX86 = ${env:ProgramFiles(x86)}
    if (-not [string]::IsNullOrWhiteSpace($programFilesX86)) {
        $windowsKitsRoot = Join-Path $programFilesX86 "Windows Kits\10\bin"
    }
    if (-not [string]::IsNullOrWhiteSpace($windowsKitsRoot) -and (Test-Path $windowsKitsRoot)) {
        $candidate = Get-ChildItem -LiteralPath $windowsKitsRoot -Recurse -Filter signtool.exe |
            Sort-Object FullName -Descending |
            Select-Object -First 1
        if ($candidate) {
            return $candidate.FullName
        }
    }

    throw "signtool.exe was not found. Install the Windows SDK or pass -SignToolPath."
}

function Invoke-CodeSigning {
    param([string]$FilePath)

    if (-not $Sign) {
        return
    }

    if ([string]::IsNullOrWhiteSpace($CertificateThumbprint)) {
        throw "Signing was requested, but no certificate thumbprint was provided. Pass -CertificateThumbprint or set STORAGEBOX_SIGN_CERT_THUMBPRINT."
    }

    $signTool = Find-SignTool
    $arguments = @(
        "sign",
        "/fd", "SHA256",
        "/td", "SHA256",
        "/sha1", $CertificateThumbprint
    )
    if (-not [string]::IsNullOrWhiteSpace($TimestampServer)) {
        $arguments += @("/tr", $TimestampServer)
    }
    $arguments += $FilePath

    Invoke-Native $signTool $arguments
}

function Remove-PackageItem {
    param(
        [string]$PackageRoot,
        [string]$RelativePath
    )

    $target = Join-Path $PackageRoot $RelativePath
    if (Test-Path $target) {
        Remove-Item -LiteralPath $target -Recurse -Force
    }
}

$QtPrefix = Resolve-ConfiguredPath `
    -Value $QtPrefix `
    -EnvironmentName "QT_ROOT_DIR" `
    -Fallback "C:\Users\Lenovo\Qt\6.8.3\msvc2022_64"

$VCRuntimeDir = Resolve-ConfiguredPath `
    -Value $VCRuntimeDir `
    -EnvironmentName "VC_RUNTIME_DIR" `
    -Fallback "D:\Visual Studio\VS\VC\Redist\MSVC\14.50.35710\x64\Microsoft.VC145.CRT"

$root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$buildDir = Join-Path $root "build"
$releaseDir = Join-Path $buildDir "Release"
$exe = Join-Path $releaseDir "StorageBoxLauncher.exe"
$packageName = "StorageBoxLauncher-v$Version-win64"
$distDir = Join-Path $root "dist"
$packageDir = Join-Path $distDir $packageName
$zipPath = Join-Path $distDir "$packageName.zip"
$hashPath = "$zipPath.sha256"
$windeployqt = Join-Path $QtPrefix "bin\windeployqt.exe"

if (-not (Test-Path $windeployqt)) {
    throw "windeployqt.exe was not found under QtPrefix: $QtPrefix"
}

Invoke-Native "cmake" @("-S", $root, "-B", $buildDir, "-DCMAKE_PREFIX_PATH=$QtPrefix")
Invoke-Native "cmake" @("--build", $buildDir, "--config", "Release")

$deployArguments = @($exe)
if (-not $IncludeTranslations) {
    $deployArguments += "--no-translations"
}
Invoke-Native $windeployqt $deployArguments

Invoke-CodeSigning $exe

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
} else {
    Write-Warning "VCRuntimeDir was not found; packaged app may require the Microsoft Visual C++ Redistributable: $VCRuntimeDir"
}

if (-not $IncludeTranslations) {
    Remove-PackageItem $packageDir "translations"
}
if (-not $IncludeSoftwareOpenGL) {
    Remove-PackageItem $packageDir "opengl32sw.dll"
}
if (-not $KeepNetworkRuntime) {
    Remove-PackageItem $packageDir "Qt6Network.dll"
    Remove-PackageItem $packageDir "networkinformation"
    Remove-PackageItem $packageDir "tls"
}

Copy-Item -LiteralPath (Join-Path $root "packaging\README_PORTABLE.md") -Destination (Join-Path $packageDir "README.txt") -Force

if (Test-Path $zipPath) {
    Remove-Item -LiteralPath $zipPath -Force
}
if (Test-Path $hashPath) {
    Remove-Item -LiteralPath $hashPath -Force
}
Compress-Archive -Path (Join-Path $packageDir "*") -DestinationPath $zipPath -Force

$hash = Get-FileHash -Algorithm SHA256 -LiteralPath $zipPath
Set-Content -LiteralPath $hashPath -Encoding UTF8 -Value "$($hash.Hash.ToLowerInvariant())  $(Split-Path $zipPath -Leaf)"

Write-Output $zipPath
Write-Output $hashPath
