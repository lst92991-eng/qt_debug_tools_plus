# Day 2：建立第一种公共数据 DataFrame

## A. 本课结果

完成后，工程会第一次拥有独立于界面的公共代码库 `mcd_sdk`。其中的 `DataFrame` 可以同时保存：

- 一帧数据的接收时间；
- 原始字节；
- 一个或多个数值通道；
- 接收或发送方向；
- 少量协议扩展属性。

本课还会创建第一个自动测试。它不打开窗口、不连接设备，直接验证数据默认值、时间戳和字段读写。

今天不创建四类插件接口。对零基础学习者来说，没有调用者的接口只会增加记忆负担。接口会在后续第一次实现协议、物理设备或页面插件时分别引入。

## B. 开始条件

必须先完成 Day 1，能够运行主窗口外壳。

### 第一次下载本课

```powershell
git clone --branch teaching-day-02-start https://github.com/lst92991-eng/qt_debug_tools_plus.git
Set-Location .\qt_debug_tools_plus
git switch -c practice/day02
git status
```

### 已经有仓库

```powershell
git fetch origin --tags
git switch -c practice/day02 teaching-day-02-start
git status
```

预期最后显示：

```text
On branch practice/day02
nothing to commit, working tree clean
```

本课会新增三个文件并修改 `CMakeLists.txt`：

```text
src/sdk/DataFrame.h
src/sdk/DataFrame.cpp
tests/sdk/DataFrameTest.cpp
CMakeLists.txt
```

## C. 本课只学三个新概念

### 1. 普通数据结构

`struct` 用来把相关字段放在一起。`DataFrame` 只保存数据，不打开设备、不解析协议、不操作界面。

### 2. 静态库

`mcd_sdk` 是一个 CMake 静态库 target。它编译成 `.lib`，主程序和以后各类插件都可以链接它，从而共享同一个数据定义。

### 3. 自动测试

测试程序调用公共代码，条件不满足时返回非零退出码。CMake 的 `add_test()` 会把程序交给 CTest；退出码 0 表示通过，非零表示失败。官方说明见 [CMake add_test](https://cmake.org/cmake/help/latest/command/add_test.html)。

## D. 创建 DataFrame.h

### D1. 用 Qt Creator 创建目录和文件

1. 选择 `文件/File > 新建文件/New File or Project`；
2. 选择 `C/C++ > C++ Header File`；
3. Name 填 `DataFrame.h`；
4. Path/路径选择仓库的 `src` 目录，并在路径末尾增加 `sdk`；
5. 如果提示创建不存在的目录，选择 Yes；
6. 完成后确认文件实际路径是 `src/sdk/DataFrame.h`；
7. 如果向导自动加入了其他模板内容，全部删掉。

### D2. 输入完整内容

```cpp
#pragma once

#include <QByteArray>
#include <QMetaType>
#include <QString>
#include <QVariantMap>
#include <QVector>

#include <limits>

enum class FrameDirection {
    Receive,
    Transmit
};

struct ChannelSample {
    quint16 index = 0;
    double value = std::numeric_limits<double>::quiet_NaN();
    QString name;
    QString unit;
};

struct DataFrame {
    qint64 timestamp_us = 0;
    QVector<ChannelSample> channels;
    QByteArray rawPayload;
    FrameDirection direction = FrameDirection::Receive;
    QVariantMap attributes;
};

Q_DECLARE_METATYPE(FrameDirection)
Q_DECLARE_METATYPE(ChannelSample)
Q_DECLARE_METATYPE(DataFrame)
Q_DECLARE_METATYPE(QVector<ChannelSample>)

void registerMcuDebugMetaTypes();
qint64 currentTimestampMicros();
```

按 `Ctrl+S` 保存。

### D3. 理解每个字段

| 字段 | 含义 | 当前默认值 |
| --- | --- | --- |
| `timestamp_us` | Unix 时间戳，单位微秒 | 0，创建真实帧时再填写 |
| `channels` | 协议解析出的数值通道列表 | 空列表 |
| `rawPayload` | 原始字节，Raw Viewer 后续使用 | 空字节数组 |
| `direction` | 接收帧或发送帧 | Receive |
| `attributes` | 协议特有的少量扩展字段 | 空 Map |

`ChannelSample::value` 默认是 NaN，意思是“当前没有可绘制的数值”。原始数据仍可放进 `rawPayload`，以后曲线模块可以跳过 NaN。

现在没有加入缓冲池 sequence、generation 或 overflow 字段。它们要等 Day 4 出现真实缓冲需求后再加入。

## E. 创建 DataFrame.cpp

### E1. 创建源文件

1. 选择 `文件/File > 新建文件/New File or Project`；
2. 选择 `C/C++ > C++ Source File`；
3. Name 填 `DataFrame.cpp`；
4. Path 选择 `src/sdk`；
5. 完成后输入下面全部内容。

```cpp
#include "sdk/DataFrame.h"

#include <QMetaType>

#include <chrono>

void registerMcuDebugMetaTypes()
{
    // 自定义类型只有注册后，才能安全地通过 Qt 的跨线程队列信号传递。
    qRegisterMetaType<FrameDirection>("FrameDirection");
    qRegisterMetaType<ChannelSample>("ChannelSample");
    qRegisterMetaType<DataFrame>("DataFrame");
    qRegisterMetaType<QVector<ChannelSample>>("QVector<ChannelSample>");
}

qint64 currentTimestampMicros()
{
    // 系统时间便于和日志、抓包工具对齐，不用于计算严格单调的超时间隔。
    using Clock = std::chrono::system_clock;
    const auto now = Clock::now().time_since_epoch();
    return static_cast<qint64>(
        std::chrono::duration_cast<std::chrono::microseconds>(now).count());
}
```

按 `Ctrl+S` 保存。

`qRegisterMetaType` 不是在解析数据，它只是告诉 Qt 元对象系统这些 C++ 类型的名字。以后物理线程把数据排队发送到主线程时会用到。

`currentTimestampMicros()` 使用系统时间，因为日志和外部抓包也使用现实世界时间。以后状态机测量超时时长时，应使用单调时钟，不能混淆两种用途。

## F. 创建第一个测试

### F1. 创建测试文件

1. 选择 `文件/File > 新建文件/New File or Project`；
2. 选择 `C/C++ > C++ Source File`；
3. Name 填 `DataFrameTest.cpp`；
4. Path 选择仓库的 `tests` 目录，并在末尾增加 `sdk`；
5. 完成后确认实际路径是 `tests/sdk/DataFrameTest.cpp`。

### F2. 输入完整测试代码

```cpp
#include "sdk/DataFrame.h"

#include <cmath>
#include <iostream>

namespace {

int fail(const char* message)
{
    std::cerr << "FAILED: " << message << '\n';
    return 1;
}

} // namespace

int main()
{
    registerMcuDebugMetaTypes();

    ChannelSample emptySample;
    if (emptySample.index != 0) {
        return fail("default channel index must be zero");
    }
    if (!std::isnan(emptySample.value)) {
        return fail("default channel value must be NaN");
    }

    DataFrame frame;
    if (frame.direction != FrameDirection::Receive) {
        return fail("default direction must be Receive");
    }

    frame.timestamp_us = currentTimestampMicros();
    frame.rawPayload = QByteArray::fromHex("0102A0");
    frame.channels.append(ChannelSample{1, 12.5, QStringLiteral("voltage"),
                                        QStringLiteral("V")});
    frame.attributes.insert(QStringLiteral("source"), QStringLiteral("day02-test"));

    if (frame.timestamp_us <= 0) {
        return fail("timestamp must be positive");
    }
    if (frame.rawPayload.toHex().toUpper() != QByteArray("0102A0")) {
        return fail("raw payload changed unexpectedly");
    }
    if (frame.channels.size() != 1 || frame.channels.first().value != 12.5) {
        return fail("channel sample was not stored");
    }
    if (frame.attributes.value(QStringLiteral("source")).toString()
        != QStringLiteral("day02-test")) {
        return fail("attribute was not stored");
    }

    std::cout << "DataFrame test passed\n";
    return 0;
}
```

这个测试不用 Qt Test 框架，目的是先看清最基本的规则：成功返回 0，失败打印原因并返回 1。等测试数量增加后再引入更完整的测试组织方式。

## G. 修改 CMakeLists.txt

### G1. 为什么必须修改

把文件放进目录并不代表 CMake 会自动编译。必须明确告诉 CMake：

- 哪些文件属于 `mcd_sdk`；
- 谁可以包含 `src` 下的头文件；
- 主程序链接哪个库；
- 测试程序如何构建和注册。

### G2. 替换完整文件

打开根目录 `CMakeLists.txt`，按 `Ctrl+A`，替换成下面完整内容：

```cmake
cmake_minimum_required(VERSION 3.20)

project(MCUDebugTool VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets)

add_library(mcd_sdk STATIC
    src/sdk/DataFrame.cpp
    src/sdk/DataFrame.h
)
target_include_directories(mcd_sdk PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(mcd_sdk PUBLIC Qt6::Core)

add_executable(mcd_app WIN32
    src/app/main.cpp
    src/app/MainWindow.cpp
    src/app/MainWindow.h
    src/app/MainWindow.ui
)
target_link_libraries(mcd_app PRIVATE mcd_sdk Qt6::Widgets)

include(CTest)
if(BUILD_TESTING)
    add_executable(mcd_sdk_data_frame_test
        tests/sdk/DataFrameTest.cpp
    )
    target_link_libraries(mcd_sdk_data_frame_test PRIVATE mcd_sdk)
    add_test(NAME sdk_data_frame COMMAND mcd_sdk_data_frame_test)
endif()
```

保存后选择 `构建/Build > Run CMake`。左侧项目树应出现：

```text
mcd_sdk
mcd_app
mcd_sdk_data_frame_test
```

`PUBLIC` include 路径会传给链接 `mcd_sdk` 的 target，因此测试可以写 `#include "sdk/DataFrame.h"`，不需要硬编码电脑绝对路径。

## H. 构建和运行测试

### H1. 构建全部目标

按 `Ctrl+B`。编译输出中应看到 `mcd_sdk`、`mcd_app` 和 `mcd_sdk_data_frame_test` 都成功生成。

先按 `Ctrl+R` 运行主程序，确认 Day 1 界面仍能打开，然后关闭窗口。

### H2. 用 CTest 运行

如果 Day 0 按推荐路径把仓库放在 `D:\Code\qt_debug_tools_plus`，并把 Qt Creator 构建目录设为 `build-qtcreator-debug`，打开 PowerShell 执行：

```powershell
Set-Location D:\Code\qt_debug_tools_plus
& 'D:\App\Qt\Tools\CMake_64\bin\ctest.exe' --test-dir '.\build-qtcreator-debug' --output-on-failure
```

如果项目在 C 盘，把命令中的 `D:\Code` 改为 `C:\Code`；如果 Qt 安装在 `C:\Qt`，把 CTest 路径改为 `C:\Qt\Tools\CMake_64\bin\ctest.exe`。

预期输出包含：

```text
Start 1: sdk_data_frame
1/1 Test #1: sdk_data_frame ........... Passed
100% tests passed, 0 tests failed out of 1
```

CTest 会运行由 `enable_testing()`/`include(CTest)` 和 `add_test()` 注册的程序，并根据退出码判断通过或失败。[CTest 官方说明](https://cmake.org/cmake/help/latest/manual/ctest.1.html)

## I. 自测

### 测试 1：确认失败能被发现

把测试中的 `frame.channels.size() != 1` 临时改成 `frame.channels.size() != 2`，保存、构建并再次运行 CTest。预期测试失败，并显示：

```text
FAILED: channel sample was not stored
```

验证完成后立刻改回 `!= 1`，再次构建和运行，必须恢复为通过。不要把故意失败的代码提交。

### 测试 2：主程序回归

运行 `mcd_app`，确认 Day 1 窗口仍能打开，“文件 > 退出”仍能关闭程序。

## J. 常见错误

### `Cannot open include file: sdk/DataFrame.h`

检查文件是否真的在 `src/sdk`，并确认 CMake 中存在：

```cmake
target_include_directories(mcd_sdk PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
```

保存 CMake 后执行 `Build > Run CMake`。

### `undefined reference` 或 `unresolved external symbol currentTimestampMicros`

`DataFrame.cpp` 没有加入 `add_library(mcd_sdk STATIC ...)`，或者测试没有链接 `mcd_sdk`。

### CTest 显示 `No tests were found`

检查 `include(CTest)`、`if(BUILD_TESTING)` 和 `add_test(...)` 是否完整，然后重新 Run CMake。还要确认 `BUILD_TESTING` 没有被设为 OFF。

### 找不到 `ctest.exe`

打开 Qt 安装目录，确认是否勾选了 CMake。缺失时运行 Qt Maintenance Tool 增加 CMake，再按实际安装路径修改命令。

### 测试失败后不知道具体原因

命令必须带 `--output-on-failure`，它会显示测试程序写入的失败信息。

## K. Git 存档

全部测试恢复通过后执行：

```powershell
git status --short
git diff --check
git add CMakeLists.txt src/sdk/DataFrame.h src/sdk/DataFrame.cpp tests/sdk/DataFrameTest.cpp
git diff --cached
git commit -m "feat(day02): add shared data frame"
git status
```

与课程标准答案比较：

```powershell
git diff teaching-day-02 -- CMakeLists.txt src/sdk/DataFrame.h src/sdk/DataFrame.cpp tests/sdk/DataFrameTest.cpp
```

没有输出表示四个文件一致。

## L. 本课小结

本课完成了一条很小但完整的路径：定义公共数据，编译成静态库，创建测试程序，再由 CTest 执行。

现在仍然没有：

- 插件接口；
- 插件装载；
- 串口、USB 或 CAN；
- 缓冲池和线程；
- 页面中的真实数据。

下一课会在出现第一个真实协议插件时引入 `IProtocolPlugin`，并学习 Qt 动态插件和 JSON manifest。
