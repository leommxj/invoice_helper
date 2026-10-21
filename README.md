# 发票小助手

一个轻量级的中国税务发票桌面工具，使用 C++17、wxWidgets 和 CMake，依赖由 vcpkg manifest 管理。

当前目录就是 Git 仓库根目录。源码、文档和构建配置进入 Git；本机下载的 vcpkg、CMake、构建产物和缓存不进入 Git。

```text
invoice_helper/
├── src/                  源码
├── docs/                 设计文档
├── scripts/              构建脚本
├── tests/                解析烟测
├── CMakeLists.txt
├── CMakePresets.json     Windows/Linux/macOS 构建预设
├── vcpkg.json            依赖清单
├── vcpkg-configuration.json
├── build/                CMake 构建产物，不进入 Git
├── tools/                本机 vcpkg/CMake，不进入 Git
└── .cache/               下载、日志和临时缓存，不进入 Git
```

项目使用 vcpkg manifest 管理依赖，不在仓库里保存手工编译的头文件和库。

## 构建环境

Windows 推荐：

- Visual Studio 2022 Build Tools，安装“使用 C++ 的桌面开发”和 Windows SDK；
- CMake 3.24+；
- Git for Windows（Git Bash）；
- vcpkg，放在 `tools/vcpkg/`。

Linux/macOS 需要 CMake、Ninja、C++17 编译器和系统图形开发包；依赖由 vcpkg 安装。

### 首次安装 vcpkg

Git Bash：

```sh
bash scripts/bootstrap-vcpkg.sh
```

PowerShell：

```powershell
scripts\bootstrap-vcpkg.ps1
```

网络需要代理时，先设置 `HTTP_PROXY`、`HTTPS_PROXY`，bootstrap 和构建脚本都会继承环境变量。

### 构建 Windows 版本

```sh
bash scripts/build.sh          # Release
bash scripts/build.sh Debug    # Debug
```

或：

```powershell
scripts\build.ps1 -Configuration Release
scripts\build.ps1 -Configuration Debug
```

首次配置会由 vcpkg 自动下载并编译 wxWidgets 和 MuPDF，之后会复用缓存。

构建结果：

```text
build/windows/bin/Release/invoice_helper.exe
build/windows/bin/Debug/invoice_helper.exe
```

Windows 使用 MSVC 静态运行时和 `x64-windows-static` 三元组，不依赖 Visual C++ 运行库 DLL。正式发行还需要打包、许可证和签名步骤。

### 跨平台构建

项目提供以下 CMake preset：

```text
windows-debug / windows-release
linux-debug / linux-release
macos-x64-debug / macos-x64-release
macos-arm64-debug / macos-arm64-release
```

例如 Linux：

```sh
cmake --preset linux-release
cmake --build --preset linux-release
```

macOS Apple Silicon：

```sh
cmake --preset macos-arm64-release
cmake --build --preset macos-arm64-release
```

跨平台注意事项：

- 依赖全部通过同一个 `vcpkg.json` 获取，平台差异由 vcpkg triplet 处理；
- Windows 使用系统 PDF 的 `print` 动词；
- Linux/macOS 使用 `lp`，没有时回退到 `lpr`，需要安装 CUPS；
- macOS 目前生成普通可执行文件，正式分发还需要 `.app` bundle、签名和 notarization；
- Linux 正式分发还需要 AppImage/deb 等打包步骤。

第三方依赖版本和许可证见 [docs/DEPENDENCIES.md](docs/DEPENDENCIES.md)。

## 当前功能

- 文件拖拽和文件选择；
- 文件夹拖入或选择，按文件夹分组管理；
- PDF、OFD、XML、PNG、JPEG、TIFF、WebP 的基础格式识别；
- PDF 文本层解析：票种、发票号码、开票日期、购买方、销售方、税号、明细、金额、税额、价税合计、大写金额、开票人和备注；
- 多选发票，固定统计面板显示张数、重复张数、金额、税额和价税合计；
- 重复发票按发票号码识别并标红，仍正常计入统计和打印；
- 打印选中发票：单张直接把 PDF 交给系统打印，多张先合并临时 PDF 再调用一次系统打印；
- 合成选中 PDF 并另存为，或合成后用系统默认 PDF 阅读器打开；
- 列表展示发票号码、开票日期和价税合计；
- 详情面板展示解析结果和明细；
- 主分割面板和右侧上下详情区可拖动调整大小；
- 窗口置顶按钮。

解析结果来自 PDF 文本层，不是税务查验结果。OFD、XML 和图片 OCR 尚未接入。

打印使用系统默认打印机。Windows 会调用 PDF 程序注册的 `print` 动词；Linux/macOS 使用 `lp` 或 `lpr`。打印机、份数和纸张方向由系统或打印程序控制。若 Windows 默认打印机是 Microsoft Print to PDF，会弹出保存文件对话框；需要打印到物理打印机时，应先把物理打印机设为系统默认打印机。多张发票合并后的临时 PDF 位于应用数据目录的 `print-cache`，超过 24 小时的文件会在后续打印时清理。

## 解析测试

```sh
build/windows/bin/Release/invoice_parser_smoke.exe "tests/invoices/发票.pdf"
```

Linux/macOS：

```sh
build/linux/bin/invoice_parser_smoke "tests/invoices/发票.pdf"
```

测试发票统一放在 `tests/invoices/`，该目录不进入 Git，避免把真实税号和交易信息带入版本历史。
