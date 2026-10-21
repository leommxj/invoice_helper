# 发票小助手

一个轻量级的中国税务发票桌面工具，使用 C++17、wxWidgets 和 CMake。

当前目录就是 Git 仓库根目录。源码、文档和构建脚本进入 Git；本机下载的构建工具、第三方库、构建产物和缓存不进入 Git。

```text
invoice_helper/
├── src/              源码
├── docs/             设计文档
├── scripts/          构建脚本
├── tests/            解析烟测
├── CMakeLists.txt
├── build/            CMake 构建产物，不进入 Git
├── third_party/      本机第三方库，不进入 Git
├── tools/            本机 CMake/Ninja，不进入 Git
└── .cache/           下载、解压和临时缓存，不进入 Git
```

项目不使用 Java、Qt、WebView、Node.js 或后台服务。

## 构建

```sh
bash scripts/build.sh
```

构建结果：

```text
build/invoice_helper.exe
```

当前脚本使用 `Release` 配置；MinGW 下会静态链接 C++ 运行时，生成物不依赖 MinGW 的 `libstdc++-6.dll`、`libgcc_s_seh-1.dll` 或 `libwinpthread-1.dll`。正式发行还需要额外的打包、许可证和签名步骤。

脚本使用本目录的 `tools/`、`third_party/`、`build/` 和 `.cache/`，不会使用系统临时目录。第三方依赖版本见 [docs/DEPENDENCIES.md](docs/DEPENDENCIES.md)。

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

打印使用 Windows 系统文件关联的 PDF 程序，需要机器上安装支持 `print` 动词的 PDF 软件。`print` 动词会使用系统默认打印机，打印机、份数、方向等由该系统程序控制。若默认打印机是 Microsoft Print to PDF，会弹出保存文件对话框；需要打印到物理打印机时，应先把物理打印机设为系统默认打印机。多张发票合并后的临时 PDF 位于应用数据目录的 `print-cache`，超过 24 小时的文件会在后续打印时清理。

## 解析测试

```sh
build/invoice_parser_smoke.exe "tests/invoices/发票.pdf"
```

测试发票统一放在 `tests/invoices/`，该目录不进入 Git，避免把真实税号和交易信息带入版本历史。
