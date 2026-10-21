# 依赖管理

项目使用 **vcpkg manifest** 管理 C++ 依赖。依赖版本固定在 `vcpkg.json` 和 `vcpkg-configuration.json` 的 baseline 中（当前 baseline：`a2f9862da6d88040a8537fb9b8cb24deaa9a196b`），不把手工编译的头文件、静态库或 Debug/Release 两套目录放进仓库。

MuPDF 的 vcpkg 端口名是 `libmupdf`，CMake 包名是 `unofficial-libmupdf`。

## 依赖版本

| 依赖 | 版本 | 用途 |
| --- | --- | --- |
| wxWidgets | 3.3.3 | 原生桌面 GUI、拖拽、数据视图和系统集成 |
| MuPDF (`libmupdf`) | 1.26.10 | PDF 文本层提取和 PDF 合并 |
| CMake | 3.24+ | 配置和构建 |
| vcpkg | 当前 baseline | 下载、编译和提供依赖 |
| MSVC 2022 / GCC / Clang | C++17 | 编译器 |

Windows 使用 `x64-windows-static` 三元组：静态库、静态 MSVC 运行时。Linux 使用 `x64-linux`，macOS 使用 `x64-osx` 或 `arm64-osx`。

## 目录约定

```text
tools/vcpkg/            本机 vcpkg，不进入 Git
build/windows/          Windows 构建目录，不进入 Git
build/linux/            Linux Release 构建目录，不进入 Git
build/linux-debug/      Linux Debug 构建目录，不进入 Git
build/windows/vcpkg_installed/  CMake manifest 安装的依赖，不进入 Git
vcpkg_installed/        手动执行 vcpkg install 时的安装目录，不进入 Git
```

CMake 在配置阶段通过以下 toolchain 接入 vcpkg：

```text
tools/vcpkg/scripts/buildsystems/vcpkg.cmake
```

Debug 和 Release 由同一份 manifest 自动提供对应配置，不再需要 `third_party/wxWidgets-debug` 之类的并行目录。

## 许可证

- wxWidgets：LGPL-2.0-or-later，带 WxWindows-exception-3.1；
- MuPDF：AGPL-3.0-only，或商业许可。

MuPDF 的 AGPL 要求对外分发本程序时提供相应源代码，或从 Artifex 获取商业许可。发布前必须确认许可证方案，或者替换 PDF 引擎。vcpkg 安装目录中的 `share/<port>/copyright` 保存各依赖的许可证文本。

## 代理

如果 GitHub 或依赖下载需要代理，在运行 bootstrap 或构建前设置：

```sh
export HTTP_PROXY=http://127.0.0.1:7890
export HTTPS_PROXY=http://127.0.0.1:7890
```

vcpkg 的下载进程会继承这些环境变量。
