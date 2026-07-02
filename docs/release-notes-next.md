# Next Release Notes

## 改进

- Windows exe 现在带有产品名、版本号、原始文件名和版权等文件属性。
- 程序会设置 Qt 应用版本，版本号来自 `CMakeLists.txt`。
- 配置文件新增 `schemaVersion` 和 `savedAtUtc`。
- 保存配置时使用原子写入，并保留最近一次 `config.backup.json`。
- 配置损坏时会保留 `config.invalid.<timestamp>.json`，然后恢复默认配置。
- 托盘菜单和盒子右键菜单新增 `开机自启动`。
- 托盘菜单和盒子右键菜单新增 `打开配置文件夹`。
- 九宫格项目目标丢失时，会在提示中说明文件可能已被移动或删除。
- 打包脚本默认裁剪翻译、软件 OpenGL、Qt Network/TLS 运行库以减小体积。
- 打包脚本支持 `-Sign` 代码签名，并会生成 `.zip.sha256` 校验文件。

## 维护说明

- Windows 资源文件由 `app.rc.in` 生成，避免 exe 版本信息和 CMake 版本漂移。
- 发布前请按 `docs/RELEASE_CHECKLIST.md` 检查版本、签名、包体积、校验文件和便携说明。
