# Debugging Guide

## Build Problems

If CMake cannot find Qt, pass the installed Qt prefix explicitly:

```powershell
cmake -S . -B build -DCMAKE_PREFIX_PATH="C:\Users\Lenovo\Qt\6.8.3\msvc2022_64"
```

If linking fails with `LNK1104` for `StorageBoxLauncher.exe`, the app is probably running:

```powershell
Stop-Process -Name StorageBoxLauncher -Force -ErrorAction SilentlyContinue
cmake --build build --config Release
```

If the app starts but reports missing Qt DLLs, deploy runtime files:

```powershell
& "C:\Users\Lenovo\Qt\6.8.3\msvc2022_64\bin\windeployqt.exe" ".\build\Release\StorageBoxLauncher.exe"
```

For Qt plugin diagnostics:

```powershell
$env:QT_DEBUG_PLUGINS="1"
.\build\Release\StorageBoxLauncher.exe
```

## Runtime State

Configuration is stored at:

```text
%APPDATA%\StorageBoxProject\Storage Box Launcher\config.json
```

The app writes a recent backup before overwriting config:

```text
%APPDATA%\StorageBoxProject\Storage Box Launcher\config.backup.json
```

If `config.json` cannot be parsed, the app keeps a timestamped copy named like `config.invalid.20260702-120000.json` and starts from defaults.

Custom box images are copied to:

```text
%APPDATA%\StorageBoxProject\Storage Box Launcher\Icons
```

To reset local state, exit the app and rename or delete the config folder. Keep a backup when debugging user data.

## Common Symptoms

### Box Is Not Visible

- Check whether the process is running:

```powershell
Get-Process StorageBoxLauncher -ErrorAction SilentlyContinue
```

- Use the tray menu to exit and restart.
- Check `config.json` for off-screen `x`/`y` coordinates after monitor changes.
- Avoid parenting boxes directly under Windows `Progman` or `WorkerW`; that can hide transparent Qt windows behind the desktop icon layer on some machines.

### Clicked Item Does Not Open

- Confirm the stored path exists.
- `.lnk` and `.url` entries are opened through `ShellExecuteW`.
- Packaged UWP/AppX shortcuts should use stable `.lnk` files when possible.

### Garbled Chinese Text

- Source files should be UTF-8.
- MSVC builds use `/utf-8` from `CMakeLists.txt`.
- In PowerShell, prefer:

```powershell
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8
```

### Startup Shortcut Issues

The tray menu and box context menu manage startup shortcuts for the current Windows user. The shortcut lives under:

```text
%APPDATA%\Microsoft\Windows\Start Menu\Programs\Startup\Storage Box Launcher.lnk
```

If toggling startup fails, check whether the app is running from a writable local path and whether security software is blocking `.lnk` creation in the Startup folder.

## Useful Inspection Commands

```powershell
git status --short
git diff --check
Get-Process StorageBoxLauncher -ErrorAction SilentlyContinue | Select-Object Id,ProcessName,WorkingSet64
```
