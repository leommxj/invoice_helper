# 本地依赖

以下内容只存在于开发机，不进入 Git：

| 依赖 | 版本 | 目录 | 用途 |
| --- | --- | --- | --- |
| CMake | 3.31.8 | `tools/cmake/` | 配置和构建 |
| Ninja | 1.12.1 | `tools/ninja/` | 构建执行器 |
| wxWidgets | 3.2.6 | `third_party/wxWidgets/` | 原生桌面 GUI、拖拽和打印 |
| MuPDF | 1.28.5 | `third_party/mupdf/` | PDF 文本层提取和 PDF 合并 |

wxWidgets 使用静态单体库构建，关闭了 WebView、示例、测试和演示程序。MuPDF 从源码构建为静态库。

MuPDF 采用 AGPL-3.0-or-later 或商业许可。对外分发本程序前必须确认许可证方案，或者替换 PDF 引擎。
