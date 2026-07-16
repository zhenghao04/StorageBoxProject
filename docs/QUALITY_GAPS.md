# Quality Gaps

This file tracks known gaps and recommended implementation order. It is intentionally pragmatic: items here should be small enough to turn into issues or follow-up commits.

## Already Improved

- GitHub Actions now performs a Windows Release build with Qt.
- `AGENTS.md` documents build, handoff, and regression expectations.
- `docs/TEST_PLAN.md` defines the current manual regression plan.
- `docs/DEBUGGING.md` documents common local failures and recovery steps.
- Configuration writes are atomic, include a schema version, and preserve unreadable files as timestamped backups.
- Duplicate paths are rejected consistently, missing targets are marked in the panel, and tray actions can show or hide every box.

## P0 Gaps

- No automated unit tests. Core path handling, JSON serialization, duplicate detection, and bounds checks are still only covered manually.
- No automated UI smoke test. Basic launch/open panel behavior should eventually be checked with a Windows UI automation tool.
- Release versioning is inconsistent: `CMakeLists.txt`, README download text, release notes, and package script defaults can drift.
- Packaging script has machine-specific defaults for Qt and MSVC runtime paths. It works locally but is not portable enough for all contributors.

## P1 Gaps

- `StorageBoxApp.cpp` owns app state, Windows integration, painting, popup UI, drag/drop, and persistence. Splitting storage, window helpers, and item model logic would make testing easier.
- Error reporting is message-box based and not logged. A small log file under app data would help user support.
- Window layering has known Windows shell edge cases around Win+D, touchpad show-desktop gestures, games, and virtual desktops.
- Startup behavior is documented but not managed in app settings.

## P2 Gaps

- High-DPI and multi-monitor behavior needs broader manual coverage.
- Accessibility is limited: keyboard-only use, screen-reader names, and focus order need attention.
- There is no installer; distribution is portable zip only.
- Custom icon copies are not garbage-collected when icons are replaced or boxes are deleted.

## Recommended Next Steps

1. Extract pure data logic into testable helpers and add CTest-based unit tests.
2. Make `tools/package_release.ps1` accept environment-driven Qt/MSVC paths and document CI-safe packaging.
3. Add optional app-managed startup toggle.
4. Revisit show-desktop behavior behind an explicit compatibility setting, not as the default window mode.
