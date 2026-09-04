# Day 1：用 Qt Designer 搭出主窗口外壳

## A. 本课结果

完成后，空白窗口会变成上位机的静态工作区：

- 左侧是设备连接区；
- 右上是数据显示和命令控制两个区域；
- 右下是活动日志；
- 顶部有“文件”和“插件”菜单；
- 底部状态栏显示当前阶段；
- 点击“退出”能关闭程序。

这一天只做界面外壳。串口、插件扫描、连接、收发和曲线都还不存在，因此相关按钮必须保持禁用，不能用假代码伪装已经实现。

## B. 开始条件

开始前必须完成 Day 0，并且已经看到“MCU Debug Tool 教学骨架”窗口。

### 第一次下载课程

在准备保存代码的目录打开 PowerShell，执行：

```powershell
git clone --branch teaching-day-01-start https://github.com/lst92991-eng/qt_debug_tools_plus.git
Set-Location .\qt_debug_tools_plus
git switch -c practice/day01
git status
```

### 已经下载过仓库

在仓库根目录打开 PowerShell，执行：

```powershell
git fetch origin --tags
git switch -c practice/day01 teaching-day-01-start
git status
```

预期看到：

```text
On branch practice/day01
nothing to commit, working tree clean
```

如果已经存在名为 `practice/day01` 的分支，不要重复执行创建命令。先确认自己是否已经做过本课。

## C. 本课只学三个新概念

### 1. 布局 Layout

布局负责自动安排控件位置和大小。窗口缩放时，布局会重新分配空间。不要拖完控件后依靠固定坐标“摆整齐”。

### 2. 对象名 objectName

界面上显示的文字可以相同，但 C++ 通过对象名找到控件。例如 `physicalCombo` 表示物理设备类型下拉框。对象名应说明职责，不能保留 `comboBox_2`、`pushButton_3`。

### 3. 信号和槽

按钮或菜单被触发时会发出信号，另一个对象的函数接收信号并执行动作。本课只连接一个真实动作：`actionExit` 触发后关闭窗口。

## D. 用 Qt Designer 修改 MainWindow.ui

### D1. 打开设计器

1. 在 Qt Creator 左侧点 `Projects/项目`；
2. 展开 `mcd_app > Source Files > src > app`；
3. 双击 `MainWindow.ui`；
4. 中央区域应切换到 Design/设计模式；
5. 如果看到 XML 文本，点击左侧 `Design/设计` 图标。

Designer 左边是 Widget Box，右边上部是 Object Inspector，右边下部是 Property Editor。

### D2. 清理 Day 0 占位内容

1. 在 Object Inspector 选中 `verticalLayout`；
2. 点击设计器工具栏的 `Break Layout/打破布局`，快捷键通常是 `Ctrl+0`；
3. 依次选中并删除 `titleLabel`、`hintLabel`、`verticalSpacerTop`、`verticalSpacerBottom`；
4. 保留 `centralWidget`、`menuBar` 和 `statusBar`。

如果误删了 `centralWidget`，立刻按 `Ctrl+Z` 撤销。

### D3. 创建左右两栏

1. 从 Widget Box 拖一个 `Group Box` 到 `centralWidget` 左侧；
2. 拖一个普通 `Widget` 到右侧；
3. 同时选中这两个控件；
4. 选择 `Form/窗体 > Lay Out Horizontally/水平布局`；
5. 在 Object Inspector 中把布局改名为 `rootLayout`。

设置两个控件的属性：

| 控件 | objectName | 关键属性 |
| --- | --- | --- |
| 左侧 Group Box | `connectionGroupBox` | title=`设备连接`，maximumWidth=`280` |
| 右侧 Widget | `workspaceWidget` | sizePolicy 的 Horizontal Policy=`Expanding` |

### D4. 放置左侧连接控件

按下面顺序拖入 `connectionGroupBox`：

1. Label；
2. Combo Box；
3. Label；
4. Combo Box；
5. Push Button；
6. Push Button；
7. Push Button；
8. Label；
9. Label；
10. Label；
11. Vertical Spacer。

设置对象名和文字：

| 类型 | objectName | text/title | 其他属性 |
| --- | --- | --- | --- |
| QLabel | `physicalLabel` | `物理设备` | 无 |
| QComboBox | `physicalCombo` | 不添加条目 | enabled=`false` |
| QLabel | `protocolLabel` | `协议解析` | 无 |
| QComboBox | `protocolCombo` | 不添加条目 | enabled=`false` |
| QPushButton | `configButton` | `配置` | enabled=`false` |
| QPushButton | `connectButton` | `连接` | enabled=`false` |
| QPushButton | `disconnectButton` | `断开` | enabled=`false` |
| QLabel | `statusLabel` | `状态：未连接` | 无 |
| QLabel | `rxCounterLabel` | `接收：0 帧` | 无 |
| QLabel | `txCounterLabel` | `发送：0 帧` | 无 |

为了让“配置”和“连接”并排：

1. 同时选中 `configButton` 和 `connectButton`；
2. 选择水平布局；
3. 把生成的布局改名为 `connectionButtonLayout`。

然后选中连接区中的所有控件和按钮行，选择垂直布局，把生成的布局改名为 `connectionLayout`。Vertical Spacer 必须位于最后，它会把控件推到顶部。

### D5. 创建右侧工作区

先从 Widget Box 拖入以下控件到 `workspaceWidget`：

1. 一个 Label；
2. 一个普通 Widget；
3. 一个 Group Box。

属性如下：

| 类型 | objectName | text/title |
| --- | --- | --- |
| QLabel | `workspaceTitleLabel` | `MCU Debug Tool 工作区` |
| QWidget | `dataAreaWidget` | 无 |
| QGroupBox | `activityGroupBox` | `活动日志` |

选中这三个控件，应用垂直布局，并把布局命名为 `workspaceLayout`。

选中 `workspaceTitleLabel`，把 font 的 Point Size 设为 `16`，Bold 设为 `true`。

### D6. 创建数据显示区和控制区

1. 向 `dataAreaWidget` 拖入两个 Tab Widget；
2. 同时选中它们并应用水平布局；
3. 把布局命名为 `dataAreaLayout`；
4. 左侧 Tab Widget 改名 `visualTabs`；
5. 右侧 Tab Widget 改名 `controlTabs`；
6. 把两个 Tab Widget 的 sizePolicy 都设为 Expanding。

两个 Tab Widget 默认可能有两个页面。每个只保留一个页面：在多余页签上点右键，选择 `Delete Page/删除页面`。

设置剩余页面：

| 父控件 | 页面 objectName | 页签文字 | 页面中的 QLabel |
| --- | --- | --- | --- |
| `visualTabs` | `visualPlaceholderPage` | `数据显示` | objectName=`visualHintLabel`，text=`Day 6 将在这里加载数据显示插件` |
| `controlTabs` | `controlPlaceholderPage` | `命令控制` | objectName=`controlHintLabel`，text=`Day 6 将在这里加载控制插件` |

分别选中两个页面中的 Label，设置 alignment 为 `AlignCenter`，再对页面应用垂直布局。

### D7. 创建活动日志

1. 向 `activityGroupBox` 拖入一个 Plain Text Edit；
2. objectName 改为 `activityLog`；
3. readOnly 设为 `true`；
4. placeholderText 填 `程序运行记录会显示在这里`；
5. 对 `activityGroupBox` 应用垂直布局，命名 `activityLayout`。

Plain Text Edit 用于持续追加普通文本，比富文本编辑器更适合日志。后续课程会增加容量限制和批量刷新。

### D8. 创建菜单和动作

在窗口顶部菜单栏的 `Type Here/在这里输入` 位置：

1. 输入 `文件(&F)`，创建文件菜单；
2. 在文件菜单下输入 `退出(&X)`；
3. 输入 `插件(&P)`，创建插件菜单；
4. 在插件菜单下输入 `重新扫描(&R)`。

在右下角 Action Editor 中设置：

| 显示文字 | objectName | enabled |
| --- | --- | --- |
| `退出(&X)` | `actionExit` | true |
| `重新扫描(&R)` | `actionRescanPlugins` | false |

把菜单对象名分别改为 `menuFile` 和 `menuPlugins`。重新扫描在 Day 3 才实现，所以现在必须禁用。

### D9. 最终对象树检查

保存前，Object Inspector 的主要结构应与下面一致。布局名称显示位置可能略有不同，但控件父子关系必须一致。

```text
MainWindow
├─ centralWidget
│  └─ rootLayout
│     ├─ connectionGroupBox
│     │  └─ connectionLayout
│     │     ├─ physicalLabel
│     │     ├─ physicalCombo
│     │     ├─ protocolLabel
│     │     ├─ protocolCombo
│     │     ├─ connectionButtonLayout
│     │     │  ├─ configButton
│     │     │  └─ connectButton
│     │     ├─ disconnectButton
│     │     ├─ statusLabel
│     │     ├─ rxCounterLabel
│     │     ├─ txCounterLabel
│     │     └─ verticalSpacer
│     └─ workspaceWidget
│        └─ workspaceLayout
│           ├─ workspaceTitleLabel
│           ├─ dataAreaWidget
│           │  └─ dataAreaLayout
│           │     ├─ visualTabs
│           │     └─ controlTabs
│           └─ activityGroupBox
│              └─ activityLayout
│                 └─ activityLog
├─ menuBar
│  ├─ menuFile
│  │  └─ actionExit
│  └─ menuPlugins
│     └─ actionRescanPlugins
└─ statusBar
```

按 `Ctrl+S` 保存 `MainWindow.ui`。

## E. 修改 MainWindow.cpp

### E1. 打开代码文件

1. 在左侧项目树双击 `src/app/MainWindow.cpp`；
2. 按 `Ctrl+A` 全选；
3. 用下面的完整内容替换；
4. 按 `Ctrl+S` 保存。

```cpp
#include "MainWindow.h"

#include "ui_MainWindow.h"

#include <QAction>
#include <QPlainTextEdit>
#include <QStatusBar>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_ui(new Ui::MainWindow)
{
    m_ui->setupUi(this);

    connect(m_ui->actionExit, &QAction::triggered, this, &QWidget::close);

    m_ui->activityLog->appendPlainText(tr("Day 1：主窗口外壳已就绪"));
    statusBar()->showMessage(tr("Day 1：尚未接入设备和插件"));
}

MainWindow::~MainWindow()
{
    delete m_ui;
}
```

`setupUi(this)` 必须先执行，因为它创建 `.ui` 中声明的控件。执行以后才能访问 `m_ui->actionExit` 和 `m_ui->activityLog`。

`connect()` 把退出动作的 `triggered` 信号连接到窗口的 `close` 函数。这里没有额外写一层只调用 `close()` 的包装函数。

`tr()` 把用户可见文字放入 Qt 翻译机制。后续用户可见文字继续使用这种写法。

本课不修改 `MainWindow.h`、`main.cpp` 和 `CMakeLists.txt`。

## F. 构建和运行

1. 看 Qt Creator 左下角，确认 Kit 仍是 Qt 6.11.0 + MSVC 2022 + x64；
2. 按 `Ctrl+B`；
3. 点击底部 `Compile Output/编译输出`；
4. 看到构建成功后按 `Ctrl+R`；
5. 缩放窗口，确认左右区域会跟随变化；
6. 打开 `文件 > 退出`，程序应关闭。

正常窗口应满足：

- 左侧宽度不超过约 280 像素；
- 三个连接按钮和两个下拉框都是灰色不可用；
- 右侧两个页签并排；
- 活动日志第一行是 `Day 1：主窗口外壳已就绪`；
- 状态栏显示 `Day 1：尚未接入设备和插件`。

## G. 自测

### 测试 1：布局

拖动窗口四个边。右侧工作区应伸缩，左侧连接区不应无限变宽，控件不应互相覆盖。

### 测试 2：未实现功能

连接、断开、配置、重新扫描全部不可点击。这是正确结果，因为对应业务尚未实现。

### 测试 3：退出信号槽

再次运行程序，通过菜单 `文件 > 退出` 关闭。程序应正常结束，Qt Creator 的 Application Output 不应出现崩溃信息。

## H. 常见错误

### `Ui::MainWindow` 没有名为 `activityLog` 的成员

`.ui` 中的 objectName 拼错，或修改后没有保存。回 Designer 核对 `activityLog`，按 `Ctrl+S`，再选择 `Build > Run CMake` 后重新构建。

### 控件叠在一起或缩放后不移动

父容器没有应用布局。检查 `centralWidget` 是否有 `rootLayout`，`connectionGroupBox` 是否有 `connectionLayout`，`workspaceWidget` 是否有 `workspaceLayout`。

### 菜单能显示，但代码找不到 `actionExit`

菜单文字不是对象名。到 Action Editor 选中“退出”，把 objectName 明确改为 `actionExit`。

### 程序运行后按钮可以点击

回到 Property Editor，把对应控件的 enabled 取消勾选。本课不写假槽函数处理这些按钮。

## I. Git 存档

测试全部通过后，在仓库根目录执行：

```powershell
git status --short
git diff --check
git add src/app/MainWindow.cpp src/app/MainWindow.ui
git diff --cached
git commit -m "feat(day01): build main window shell"
git status
```

最后应看到工作区干净。你自己的练习分支不需要创建官方 `teaching-day-01` 标签，课程仓库已经提供标准答案标签用于比较。

比较自己的结果与标准答案：

```powershell
git diff teaching-day-01 -- src/app/MainWindow.cpp src/app/MainWindow.ui
```

没有输出表示文件完全一致；有输出时逐段核对，不要直接覆盖自己的文件。

## J. 本课小结

本课新增的是界面结构，不是设备功能。现在已经有清楚的连接区、数据区、控制区和日志区，后续模块有明确的挂载位置。

仍然简化的内容：

- 下拉框没有插件数据；
- 连接按钮不能操作；
- 页签只有占位文字；
- 日志只有启动记录；
- 没有后台线程和硬件访问。

下一课将定义最底层的数据对象和插件接口，但不会立即连接硬件。
