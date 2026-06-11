# Storage Box Launcher

这是 Windows 便携版。解压后直接运行：

```text
StorageBoxLauncher.exe
```

## 使用方式

- 左键点击盒子：展开应用九宫格
- 拖动盒子：移动位置
- 右键盒子：添加应用、重命名、新建盒子、删除盒子、切换置顶、退出
- 双击盒子：添加应用
- 拖入 `.exe`、`.lnk`、`.url` 或脚本文件：收纳到盒子中
- 在九宫格内拖动应用：调整顺序

配置会保存在当前 Windows 用户的应用数据目录中，升级新版时可以直接覆盖程序文件。

## 开机启动

如果需要开机自启动，可以把 `StorageBoxLauncher.exe` 的快捷方式放入：

```text
%APPDATA%\Microsoft\Windows\Start Menu\Programs\Startup
```

## 说明

这个压缩包已经包含 Qt 运行时和 MSVC 运行时 DLL，普通 Windows 电脑解压后即可运行。
