# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 项目概述

OcctImgui 是一个基于 OpenCASCADE 的 3D CAD 应用程序,集成了 ImGui 界面和 GLFW 窗口系统。项目采用 MVVM 架构模式,支持 STEP 模型导入、网格化处理和可视化。

## 环境配置

### 本地 vcpkg 环境部署

项目使用**本地 vcpkg**（而非全局 vcpkg）来管理依赖，避免全局环境污染。配置在 `CMakePresets.json` 中。

#### 1. 克隆 vcpkg 到项目目录

```bash
# 在项目根目录下克隆 vcpkg
git clone https://github.com/microsoft/vcpkg.git vcpkg

# Windows: 运行 bootstrap 脚本
.\vcpkg\bootstrap-vcpkg.bat

# Linux/macOS: 运行 bootstrap 脚本
./vcpkg/bootstrap-vcpkg.sh
```

#### 2. 安装项目依赖

```bash
# 使用本地 vcpkg 安装依赖（Windows x64）
.\vcpkg\vcpkg install opencascade imgui glfw3 netgen boost-signals2 boost-test nfd spdlog libigl eigen3 --triplet x64-windows

# Linux 使用 x64-linux triplet
./vcpkg/vcpkg install opencascade imgui glfw3 netgen boost-signals2 boost-test nfd spdlog libigl eigen3 --triplet x64-linux
```

**关键依赖包:**
- `opencascade` - 3D 几何内核
- `netgen` - 网格生成
- `imgui` - ImGui 界面库
- `glfw3` - 窗口管理
- `boost-signals2`, `boost-test` - Boost 库
- `spdlog` - 日志库
- `libigl`, `eigen3` - 几何处理
- `nfd` - 原生文件对话框

#### 3. vcpkg 目录结构

本地 vcpkg 环境结构:
```
OcctImgui/
├── vcpkg/                    # 本地 vcpkg 根目录（不提交到 git）
│   ├── vcpkg.exe            # vcpkg 可执行文件
│   ├── scripts/
│   │   └── buildsystems/
│   │       └── vcpkg.cmake  # CMake 工具链文件
│   ├── installed/           # 已安装的包
│   └── packages/            # 包构建缓存
├── CMakePresets.json        # CMake 预设配置
└── .gitignore               # 应包含 vcpkg/ 目录
```

**重要:** 确保 `.gitignore` 包含 `vcpkg/` 以避免提交大型二进制文件。

### CMake Presets 配置

项目使用 CMake Presets (CMake 3.15+) 简化配置，采用继承式架构设计:

**基础预设:**
- `base`: 隐藏的基础预设，包含所有公共配置（Visual Studio 2022, x64, vcpkg 工具链）

**可用的配置预设:**
- `debug`: Debug 模式（无优化，完整调试信息）
- `relwithdebinfo`: RelWithDebInfo 模式（优化但包含调试信息，用于性能分析）
- `release`: Release 模式（完全优化，用于生产发布）

**可用的构建预设:**
- `debug`: 使用 debug 预设构建
- `relwithdebinfo`: 使用 relwithdebinfo 预设构建
- `release`: 使用 release 预设构建

**输出目录:**
- Debug: `build/Debug/`
- RelWithDebInfo: `build/RelWithDebInfo/`
- Release: `build/Release/`

## 构建和测试

### 构建项目

#### 方法 1: 使用 CMake Presets（推荐）

```bash
# 配置项目（开发调试使用）
cmake --preset debug

# 或使用 RelWithDebInfo（性能分析）
cmake --preset relwithdebinfo

# 或使用 Release（生产发布）
cmake --preset release

# 构建（使用对应的构建预设）
cmake --build --preset debug

# 或构建其他配置
cmake --build --preset relwithdebinfo
cmake --build --preset release
```

#### 方法 2: 传统 CMake 方式

```bash
# 配置（手动指定工具链文件）
cmake -DCMAKE_TOOLCHAIN_FILE=./vcpkg/scripts/buildsystems/vcpkg.cmake -DCMAKE_CXX_STANDARD=17 -B build

# 构建
cmake --build build --config Debug
# 或
cmake --build build --config Release
```

#### 输出目录

- 可执行文件: `build/bin/Debug/` 或 `build/bin/Release/`
- 库文件: `build/lib/Debug/` 或 `build/lib/Release/`

### 运行测试

```bash
# 运行所有测试
cd build
ctest

# 运行详细输出的测试
ctest -V

# 运行特定测试
ctest -R model_test
ctest -R geometry_model_test

# 直接运行测试可执行文件(获得更详细的输出)
./bin/Debug/model_test --log_level=all
./bin/Debug/mesh_datasource_test --log_level=all
```

### 添加新测试

1. 在 `tests/` 目录创建测试文件 (如 `my_feature_test.cpp`)
2. 在 `CMakeLists.txt` 中添加: `add_boost_test(my_feature_test tests/my_feature_test.cpp)`
3. 测试使用 Boost.Test 框架,测试数据目录定义为 `MESH_TEST_DATA_DIR`

## 架构概述

### MVVM 架构

项目严格遵循 MVVM (Model-View-ViewModel) 模式:

```
Application
    └── ApplicationBootstrapper (初始化和依赖注入)
            ├── WindowManager (GLFW 窗口管理)
            ├── InputManager (键盘/鼠标输入)
            ├── ModelManager + ModelFactory (模型层)
            ├── ViewModelManager (ViewModel 层)
            └── ViewManager (View 层: ImGuiView + OcctView)
```

**关键组件职责:**

- **Model** (`src/model/`): 业务逻辑和数据,不依赖 UI
  - `IModel`: 通用模型接口
  - `GeometryModel`: 几何模型,管理 OpenCASCADE 形状
  - `ModelManager`: 管理多个模型实例
  - `ModelImporter`: 导入 STEP 文件

- **ViewModel** (`src/viewmodel/`): 连接 Model 和 View,处理 UI 逻辑
  - `IViewModel`: ViewModel 接口
  - `GeometryViewModel`: 几何模型的 ViewModel,暴露 Property 给 View
  - `ViewModelManager`: 管理 ViewModel 生命周期

- **View** (`src/view/`): UI 展示层
  - `IView`: View 接口
  - `ImGuiView`: ImGui 界面视图
  - `OcctView`: OpenCASCADE 3D 视图
  - `ViewManager`: 管理多个视图

- **ApplicationBootstrapper** (`src/ApplicationBootstrapper.h`):
  - 初始化所有组件并建立连接
  - 管理依赖注入和生命周期
  - 按顺序初始化: Window -> Input -> Model -> ViewModel -> Views

### 响应式编程系统

项目实现了基于 Boost.Signals2 的响应式编程系统:

**Property 系统** (`src/mvvm/Property.h`):
- `Property<T>`: 带变更通知的属性类
- 支持属性绑定 (`bindTo`) 和计算属性 (`bindComputed`)
- ViewModel 通过 Property 暴露状态给 View
- 示例: `Property<int> displayMode{0}`, `Property<bool> hasSelection{false}`

**Signal 系统** (`src/mvvm/Signal.h`):
- `Signal<Args...>`: 类型安全的事件系统
- `ScopedConnection`: RAII 风格的连接管理
- `ConnectionTracker`: 批量管理多个连接
- 用于组件间的紧密耦合通信

**MessageBus 系统** (`src/mvvm/MessageBus.h`):
- 单例模式的集中式消息总线
- 消息类型: `ModelChanged`, `SelectionChanged`, `ViewChanged`, `CommandExecuted`
- `Subscription`: 自动管理订阅生命周期
- 用于系统级别的松散耦合通信

**使用场景:**
- Property: ViewModel 向 View 暴露响应式状态
- Signal: View 事件 -> ViewModel 命令,紧密相关组件通信
- MessageBus: 跨模块的系统事件广播

### 日志系统

项目使用基于 spdlog 的分层日志系统 (`src/utils/Logger.h`):

- **获取 Logger**: `Utils::Logger::getLogger("module.name")`
- **预定义 Logger**: `getAppLogger()`, `getViewManagerLogger()`, `getMvvmLogger()`
- **日志级别**: `trace`, `debug`, `info`, `warn`, `error`, `critical`
- **函数作用域日志**: `LOG_FUNCTION_SCOPE(logger, "functionName")`
- **上下文 ID**: `logger->setContextId("session-123")` 用于跟踪特定会话

**重要规则**:
- 日志消息必须使用英文 (来自 `.cursor/rules/log-rule.mdc`)
- 日志系统使用 Meyer's Singleton 模式确保静态初始化安全

### SelectionManager

`src/mvvm/SelectionManager.h`:
- 管理 3D 视图中的对象选择
- 与 OpenCASCADE 的 `AIS_InteractiveContext` 集成
- 通过 MessageBus 发布 `SelectionChanged` 事件

## 依赖库

- **OpenCASCADE**: 3D 几何内核
- **Netgen**: 网格生成库
- **ImGui**: 即时模式 GUI
- **GLFW**: 跨平台窗口和输入
- **Boost**: Signals2 (响应式编程), unit_test_framework (测试)
- **spdlog**: 日志库
- **libigl + Eigen3**: 几何处理
- **nfd**: 原生文件对话框

所有依赖通过 vcpkg 管理。

## 编码规范

### C++ 规范

- 使用 C++17 标准
- 遵循 Basic Rules: 无 goto, 避免全局变量, 避免魔术数字, 总是初始化变量
- 使用 `std::optional<T>` 代替魔术值表示可选值
- 遵循 "Make impossible states unrepresentable" 原则
- 嵌套深度不超过 3 层 (if-else, for, while)
- 枚举使用 `enum class`,采用大驼峰命名
- 函数不超过 100 行
- 数据成员不公开 (使用访问器)

### 命名约定

- 成员变量: `my` 前缀 (如 `myLogger`, `myModel`)
- OpenCASCADE 句柄: `Handle(ClassName)` 或 `opencascade::handle<ClassName>`
- 接口类: `I` 前缀 (如 `IModel`, `IView`, `IViewModel`)

### CMake 规范

- 使用现代 CMake 风格 (CMake 3.15+)
- 目标导入: `find_package` + `target_link_libraries`
- 避免使用 `include_directories` 全局设置,优先使用 `target_include_directories`

## 重要原则

来自全局 CLAUDE.md 的编程八荣八耻:
- 以暗猜接口为耻,以认真查询为荣
- 以模糊执行为耻,以寻求确认为荣
- 以臆想业务为耻,以人类确认为荣
- 以创造接口为耻,以复用现有为荣
- 以跳过验证为耻,以主动测试为荣
- 以破坏架构为耻,以遵循规范为荣
- 以假装理解为耻,以诚实无知为荣
- 以盲目修改为耻,以谨慎重构为荣

**工作流程:**
1. 先理解需求,再给出方案
2. 方案讨论优先 - 达成一致后再编码
3. 编码守则: 理解现有代码 -> 找集成点 -> 最小改动 -> 确保向后兼容
4. 遵循 TDD (测试驱动开发)
5. 问题反馈时进行根因分析,不做定向优化

## 常见任务

### 添加新的 Model

1. 继承 `IModel` 接口 (`src/model/IModel.h`)
2. 在 `ModelFactory` 中注册新模型类型
3. 实现 `getAllEntityIds()` 和 `removeEntity()` 方法
4. 使用 `notifyChange()` 通知模型变更

### 添加新的 ViewModel

1. 继承 `IViewModel` 接口 (`src/viewmodel/IViewModel.h`)
2. 使用 `Property<T>` 暴露状态给 View
3. 订阅 Model 的变更事件
4. 通过 MessageBus 发布系统事件

### 添加新的 View

1. 继承 `IView` 接口 (`src/view/IView.h`)
2. 实现 `initialize`, `newFrame`, `render`, `shutdown` 方法
3. 在 `ApplicationBootstrapper::initializeViews()` 中注册
4. 连接到 ViewModel 的 Property 信号以响应状态变更

### 调试技巧

- 使用 `LOG_FUNCTION_SCOPE` 跟踪函数调用
- 在关键位置添加 `logger->debug()` 输出
- 构建类型宏: `OCCTIMGUI_DEBUG`, `OCCTIMGUI_RELWITHDEBINFO`, `OCCTIMGUI_RELEASE`
- Boost.Test 参数: `--log_level=all`, `--run_test=<test_name>`, `--show_progress`

## 注意事项

- 不要在未咨询的情况下修改代码 (来自 `.cursor/rules/agent-general.mdc`)
- 使用中文回复用户
- 文档更新时添加版本号
- 新增功能需要编写对应的单元测试
- 保持 MVVM 架构的分层清晰,避免跨层直接依赖

---

**文档版本:** v1.2
**更新日期:** 2025-10-23
**更新内容:** 优化 CMake Presets 配置，采用现代化继承式架构（base 预设 + Visual Studio 2022），添加 Debug/RelWithDebInfo/Release 三种完整构建配置
