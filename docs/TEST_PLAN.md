# Test Plan

This project currently relies on a Windows manual regression pass plus a GitHub Actions build check. The plan below is ordered by priority.

## P0 Build And Launch

- Configure and build Release:

```powershell
cmake -S . -B build -DCMAKE_PREFIX_PATH="C:\Users\Lenovo\Qt\6.8.3\msvc2022_64"
cmake --build build --config Release
```

- Stop any previous process before rebuilding:

```powershell
Stop-Process -Name StorageBoxLauncher -Force -ErrorAction SilentlyContinue
```

- Launch `build\Release\StorageBoxLauncher.exe`.
- Confirm at least one box appears.
- Confirm the tray icon appears and the process remains running after all windows are closed.
- Confirm `StorageBoxLauncher.exe` file properties include version/product metadata after a Release build.
- Confirm boxes appear promptly on startup before any delayed config save work is noticeable.

## P0 Core Launcher Behavior

- Left click a box: panel opens.
- Left click the same box again: panel closes.
- Click occupied slots: apps/files/folders open with Windows default handlers.
- Add files/apps through `+` and through right-click `添加项目`.
- Add folders through `添加文件夹`.
- Drag files, folders, documents, shortcuts, and executables from Explorer onto the box.
- Drag items into specific empty panel slots.
- Try adding more than 9 items and confirm the full-box warning.
- Try adding a duplicate path and confirm it is ignored.

## P0 Persistence

- Move a box and restart the app; position should persist.
- Resize a box and restart the app; size should persist.
- Rename a box and restart the app; name should persist.
- Rename, remove, and reorder items; changes should persist.
- Change theme color and custom image icon; changes should persist.
- Inspect config at:

```text
%APPDATA%\StorageBoxProject\Storage Box Launcher\config.json
```

- Confirm `schemaVersion` is present in `config.json`.
- Confirm `config.backup.json` is created after a second save.
- Temporarily corrupt `config.json`, launch the app, and confirm a `config.invalid.<timestamp>.json` copy is kept while the app restores a default box.

## P0 Startup And Maintenance

- Use the tray menu to enable `开机自启动`; confirm this shortcut is created:

```text
%APPDATA%\Microsoft\Windows\Start Menu\Programs\Startup\Storage Box Launcher.lnk
```

- Disable `开机自启动`; confirm the shortcut is removed.
- Use the box right-click menu to toggle `开机自启动` and repeat the shortcut check.
- Use `打开配置文件夹` from the tray menu and box menu; confirm the app data folder opens.

## P1 Window Layering

- Default non-topmost mode: open another app over the desktop; boxes should not stay above it.
- Topmost mode: enable `置顶显示`; boxes should stay visible above ordinary windows.
- Disable topmost mode again; boxes should return to normal window layering.
- Show desktop with Win+D or touchpad gesture; known behavior can vary by Windows shell version. Do not use the experimental desktop-child-window approach without a feature flag.

## P1 Visual And Interaction

- Hover a box; highlight should appear without layout shift.
- Drag files over a box; dashed drop hint should appear.
- Resize to minimum and maximum sizes; text and count pill should remain readable.
- Open the 3x3 panel at different box sizes; panel should align below the box.
- Verify long Chinese and English names are elided, not clipped badly.

## P1 Packaging Smoke

```powershell
powershell -ExecutionPolicy Bypass -File tools\package_release.ps1 -Version 1.0.2
```

- Extract the generated zip under `dist/`.
- Run `StorageBoxLauncher.exe` from the extracted folder on a clean Windows user profile if possible.
- Confirm Qt/MSVC runtime DLLs are included.
- Confirm `README.txt` displays Chinese correctly in Notepad.
- Confirm the `.zip.sha256` file is generated.
- Confirm the default package does not include `opengl32sw.dll`, `translations`, `Qt6Network.dll`, `networkinformation`, or `tls`.
- Build a compatibility package with `-IncludeSoftwareOpenGL -KeepNetworkRuntime -IncludeTranslations` and confirm those runtime files are retained.

## P2 Exploratory Checks

- Use `.lnk` shortcuts, `.url` shortcuts, batch files, Office files, PDFs, images, and directories.
- Delete or move a stored target and confirm the app shows a clear error when clicked.
- Delete or move a stored target and confirm its grid button turns into the missing-item visual state with a warning badge.
- Delete or move a custom image icon source; the copied icon under app data should still work.
- Test high-DPI scaling at 125%, 150%, and mixed monitor setups.

## P2 Theme And Settings Checks

- Open `设置...` from the tray menu.
- Open `界面设置...` from the box `外观` menu.
- Switch between `清透浅色`, `深色玻璃`, `暖砂柔光`, and `极简白`; confirm the popup and box chrome update immediately.
- Restart the app and confirm the selected interface theme persists.
- Confirm the settings panel can toggle `置顶显示` and `开机自启动`.
- Open and close the 3x3 panel several times; confirm the fade/scale animation feels quick and does not leave ghost windows.
