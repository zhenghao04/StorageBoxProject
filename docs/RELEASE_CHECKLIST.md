# Release Checklist

Use this checklist before publishing a GitHub Release zip.

## Version Metadata

- Confirm `project(StorageBoxLauncher VERSION ...)` in `CMakeLists.txt` matches the release tag.
- Build Release and inspect `StorageBoxLauncher.exe` file properties.
- Confirm `FileDescription`, `ProductName`, `OriginalFilename`, `FileVersion`, and `ProductVersion` are populated.

## Build And Package

```powershell
powershell -ExecutionPolicy Bypass -File tools\package_release.ps1 -Version 1.0.2
```

The default package trims:

- Qt translations
- `opengl32sw.dll`
- Qt Network/TLS runtime files

Use a compatibility package when older machines need the broader Qt runtime:

```powershell
powershell -ExecutionPolicy Bypass -File tools\package_release.ps1 -Version 1.0.2 -IncludeSoftwareOpenGL -KeepNetworkRuntime -IncludeTranslations
```

## Code Signing

When a signing certificate is available:

```powershell
powershell -ExecutionPolicy Bypass -File tools\package_release.ps1 -Version 1.0.2 -Sign -CertificateThumbprint "<thumbprint>"
```

You can also set:

```powershell
$env:STORAGEBOX_SIGN_CERT_THUMBPRINT="<thumbprint>"
$env:SIGNTOOL_EXE="C:\Path\To\signtool.exe"
```

Verify after packaging:

```powershell
Get-AuthenticodeSignature dist\StorageBoxLauncher-v1.0.2-win64\StorageBoxLauncher.exe
```

## Checksums

The packaging script writes:

```text
dist\StorageBoxLauncher-v1.0.2-win64.zip.sha256
```

Attach both the `.zip` and `.zip.sha256` files to the GitHub Release.

## Portable Smoke Test

- Extract the zip into a fresh folder.
- Run `StorageBoxLauncher.exe`.
- Confirm the tray icon appears.
- Confirm a default box appears on first launch.
- Add a file/app and a folder.
- Move and resize the box, exit, restart, and confirm state persists.
- Toggle `开机自启动`, confirm the Startup shortcut is created, then turn it off again.
- Open the packaged `README.txt` in Notepad and confirm Chinese text displays correctly.
