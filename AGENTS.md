# AGENTS.md

This repository contains a Windows desktop launcher built with C++17 and Qt Widgets. The primary user-facing language is Chinese, so keep user documentation and UI copy in Chinese unless a file is already English-only.

## Project Shape

- App entry point: `src/main.cpp`
- Main application/UI logic: `src/StorageBoxApp.cpp`
- Public app/window declarations: `src/StorageBoxApp.h`
- Qt resources and app icon: `assets/`
- Windows version resource template: `app.rc.in`
- Release packaging script: `tools/package_release.ps1`
- Portable user note copied into release zips: `packaging/README_PORTABLE.md`

## Build And Verify

Use PowerShell on Windows.

```powershell
cmake -S . -B build -DCMAKE_PREFIX_PATH="C:\Users\Lenovo\Qt\6.8.3\msvc2022_64"
cmake --build build --config Release
```

If the app is already running, stop it before rebuilding because Windows will lock `build\Release\StorageBoxLauncher.exe`:

```powershell
Stop-Process -Name StorageBoxLauncher -Force -ErrorAction SilentlyContinue
```

Run the app from:

```powershell
.\build\Release\StorageBoxLauncher.exe
```

## Coding Guidelines

- Prefer existing Qt Widgets patterns in `StorageBoxApp.cpp` over adding new frameworks.
- Keep Windows-specific behavior behind `#ifdef Q_OS_WIN`.
- Use `QStringLiteral` for user-visible static strings.
- Preserve UTF-8 source encoding; the CMake file already enables `/utf-8` for MSVC.
- Keep `build/`, `dist/`, `.vs/`, `.vscode/`, and generated packages out of git.
- Do not reintroduce the experimental `Progman`/desktop-child-window approach for non-topmost boxes without a dedicated compatibility toggle and manual test coverage.

## Manual Regression Checklist

Before handing off a behavioral change, run the build and at least smoke-test:

- First launch creates a default box and writes config.
- Left click opens/closes the 3x3 panel.
- Dragging a box moves it and persists position.
- Dragging edges/corners resizes it and persists size.
- Dragging files, folders, documents, shortcuts, and apps into a box works.
- Reordering items in the panel works.
- Custom box color and image icon persist after restart.
- Non-topmost mode does not cover normal apps; topmost mode intentionally does.

See `docs/TEST_PLAN.md` for the fuller plan.

For release packaging, signing, checksum, and runtime-trimming expectations, see `docs/RELEASE_CHECKLIST.md`.
