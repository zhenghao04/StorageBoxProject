# Storage Box Launcher

[![CI](https://github.com/zhenghao04/StorageBoxProject/actions/workflows/ci.yml/badge.svg)](https://github.com/zhenghao04/StorageBoxProject/actions/workflows/ci.yml)

一个 Windows 桌面“启动器收纳盒”原型，使用 C++ 和 Qt Widgets 实现。

## 直接下载使用

普通用户不需要安装 Qt 或编译源码。到 GitHub Releases 下载：

```text
StorageBoxLauncher-v0.1.1-win64.zip
```

说明：`main` 分支包含 v0.1.1 之后的开发改动。正式可下载版本以 GitHub Releases 页面为准。

解压后直接运行：

```text
StorageBoxLauncher.exe
```

## 功能

- 支持多个悬浮收纳盒
- 每个盒子最多收纳 9 个项目：应用、快捷方式、脚本、文件夹或文档文件
- 左键点击盒子展开圆角 3x3 项目面板
- 点击面板里的项目会用 Windows 默认方式打开
- 拖动盒子改变位置，拖动盒子边缘/角落调整大小，并自动保存
- 右键盒子可添加文件/应用、添加文件夹、重命名、调整外观、新建盒子、删除盒子、切换置顶和退出
- 支持为每个盒子切换主题色，或选择本地图片作为盒子图标
- 默认不置顶，Windows 下会作为普通非置顶窗口，打开其他应用时不会浮在最上层遮挡
- 九宫格会自动读取 `.exe`、`.lnk`、`.url`、文件夹和文档等项目的系统图标
- 支持把桌面或资源管理器里的文件、文件夹、文档、快捷方式直接拖到盒子/九宫格里添加
- 支持在九宫格内拖动项目调整顺序
- 配置保存到 Qt 的 `AppDataLocation`，通常位于 `%APPDATA%\StorageBoxProject\Storage Box Launcher\config.json`
- 自带应用图标，已接入窗口、托盘、快捷方式和 Windows exe 资源

## 文档

- 维护者/Agent 说明：[AGENTS.md](AGENTS.md)
- 测试计划：[docs/TEST_PLAN.md](docs/TEST_PLAN.md)
- 调试指南：[docs/DEBUGGING.md](docs/DEBUGGING.md)
- 质量缺口和后续优先级：[docs/QUALITY_GAPS.md](docs/QUALITY_GAPS.md)
- Release notes：[docs](docs)

## 构建

需要安装 Qt 6 或 Qt 5，以及可用的 C++ 编译器。

```powershell
cmake -S . -B build -DCMAKE_PREFIX_PATH="C:\Qt\6.8.3\msvc2022_64"
cmake --build build --config Release
```

如果 Qt 已经加入 CMake 搜索路径，可以省略 `CMAKE_PREFIX_PATH`：

```powershell
cmake -S . -B build
cmake --build build --config Release
```

生成后运行：

```powershell
.\build\Release\StorageBoxLauncher.exe
```

如需把 Qt 运行时 DLL 复制到 Release 目录，执行：

```powershell
& "C:\Users\Lenovo\Qt\6.8.3\msvc2022_64\bin\windeployqt.exe" ".\build\Release\StorageBoxLauncher.exe"
```

## 开机启动

如需开机自启动，把 `StorageBoxLauncher.exe` 的快捷方式放到：

```text
%APPDATA%\Microsoft\Windows\Start Menu\Programs\Startup
```

删除快捷方式即可取消开机启动。

## 打包发布

生成可直接分发的 Windows 便携包：

```powershell
powershell -ExecutionPolicy Bypass -File tools\package_release.ps1 -Version 0.1.1
```

产物位于：

```text
dist\StorageBoxLauncher-v0.1.1-win64.zip
```

## CI 检查

GitHub Actions 会在 `main` 推送和 Pull Request 时执行 Windows Release 构建，检查 Qt/MSVC 环境下是否能产出 `StorageBoxLauncher.exe`，并运行一次 `windeployqt` 部署烟测。

## 图标资源

图标源文件位于：

```text
assets\app_icon.svg
```

生成 Windows 图标和 Qt 运行时 PNG：

```powershell
python tools\generate_icon.py
```
