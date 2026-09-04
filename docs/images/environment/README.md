# 环境安装截图清单

截图统一保存为 PNG，不拍摄或暴露邮箱、密码、令牌、机器序列号和其他个人信息。出现账号输入页面时，只截页面标题和非敏感选项，输入框内容必须遮挡或避开。

使用 `Win + Shift + S` 截取当前安装器窗口，保存到本目录。文件名固定如下：

## 系统

- `01-windows-system.png`：Windows 版本和 x64 系统类型；

## Visual Studio 2022

- `10-vs-bootstrapper.png`：VS 2022 Community 引导程序文件；
- `11-vs-cpp-workload.png`：“使用 C++ 的桌面开发”工作负载；
- `12-vs-component-details.png`：MSVC v143、Windows SDK 和 CMake 工具；
- `13-vs-install-complete.png`：安装完成页面；
- `14-msvc-cl-version.png`：x64 Native Tools 中的 `cl` 输出。

## Qt

- `20-qt-mirror-installer.png`：清华镜像中的 Windows x64 在线安装器；
- `21-qt-mirror-command.png`：带 `--mirror` 的启动命令；
- `22-qt-account-page.png`：Qt Account 页面，不得包含账号或密码；
- `23-qt-install-path.png`：安装路径 `D:\QT`；
- `24-qt-msvc-component.png`：Qt 6.11.0 的 MSVC 2022 64-bit 组件；
- `25-qt-tools-components.png`：Qt Creator、CMake 和 Ninja；
- `26-qt-install-complete.png`：Qt 安装完成页面；
- `27-qt-kit-summary.png`：Qt Creator 中无红色错误的 Desktop Kit。

## VS Code

- `30-vscode-install-options.png`：资源管理器菜单、文件关联和 PATH 选项；
- `31-vscode-official-extensions.png`：Qt Company 与 Microsoft 官方扩展；
- `32-vscode-register-qt.png`：注册 `D:\QT`；
- `33-vscode-select-kit.png`：选择 Qt 6.11.0 MSVC 2022 x64 Kit；
- `34-vscode-build.png`：CMake Configure/Build 成功。

## 新建工程

- `40-new-project-template.png`：Qt Widgets Application 模板；
- `41-project-location.png`：工程名称和路径；
- `42-build-system.png`：CMake 选择；
- `43-main-window-class.png`：MainWindow/QMainWindow 和 Generate form；
- `44-kit-selection.png`：只选 MSVC 2022 64-bit Kit；
- `45-first-build.png`：第一次 Build finished；
- `46-empty-main-window.png`：学生亲手生成的空白窗口；
- `47-step01-backup.png`：`MCUDebugTool-step01-empty` 源码备份。

每拍完一张先打开检查：文字必须能看清，窗口边缘不要被裁断，个人信息不能出现。文档确认采用后再提交原始 PNG。
