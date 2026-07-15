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

## P0 Core Launcher Behavior

- Left click a box: panel opens.
- Left click the same box again: panel closes.
- Click occupied slots: apps/files/folders open with Windows default handlers.
- Add files/apps through `+` and through right-click `添加项目`.
- Add folders through `添加文件夹`.
- Drag files, folders, documents, shortcuts, and executables from Explorer onto the box.
- Drag items into specific empty panel slots.
- Try adding more than 9 items and confirm the full-box warning.
- Try adding a duplicate path through the add dialog and by dragging it in; confirm it is not added twice.
- Delete or move a stored target; confirm its slot shows a warning icon and a "路径不存在" tooltip.
- With a panel open, click another box, an empty area of the app, and press Esc; confirm the panel closes. Open the add or item context menu and confirm it remains usable.

## P0 Persistence

- Move a box and restart the app; position should persist.
- Resize a box and restart the app; size should persist.
- Rename a box and restart the app; name should persist.
- Rename, remove, and reorder items; changes should persist.
- Change theme color and custom image icon; changes should persist.
- Confirm `schemaVersion` is written to the config.
- Replace the config contents with invalid JSON, start the app, and confirm a timestamped `.corrupt-*.json` copy is retained before the default box is created. Restore the original config after the check.
- Inspect config at:

```text
%APPDATA%\StorageBoxProject\Storage Box Launcher\config.json
```

## P1 Window Layering

- Default non-topmost mode: open another app over the desktop; boxes should not stay above it.
- Topmost mode: enable `置顶显示`; boxes should stay visible above ordinary windows.
- Disable topmost mode again; boxes should return to normal window layering.
- Show desktop with Win+D or touchpad gesture; known behavior can vary by Windows shell version. Do not use the experimental desktop-child-window approach without a feature flag.
- Use the tray menu to hide all boxes, then show all boxes again. Confirm the boxes return and remain non-topmost when topmost mode is disabled.

## P1 Visual And Interaction

- Hover a box; highlight should appear without layout shift.
- Drag files over a box; dashed drop hint should appear.
- Resize to minimum and maximum sizes; text and count pill should remain readable.
- Place boxes near each edge of every monitor. Confirm boxes remain on-screen and the 3x3 panel opens above the box when there is not enough room below it.
- Verify long Chinese and English names are elided, not clipped badly.

## P1 Packaging Smoke

```powershell
powershell -ExecutionPolicy Bypass -File tools\package_release.ps1 -Version 0.1.2
```

- Extract the generated zip under `dist/`.
- Run `StorageBoxLauncher.exe` from the extracted folder on a clean Windows user profile if possible.
- Confirm Qt/MSVC runtime DLLs are included.

## P2 Exploratory Checks

- Use `.lnk` shortcuts, `.url` shortcuts, batch files, Office files, PDFs, images, and directories.
- Delete or move a custom image icon source; the copied icon under app data should still work.
- Test high-DPI scaling at 125%, 150%, and mixed monitor setups.
