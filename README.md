# 7Zip-Gui-Mint-Linux

Linux 压缩包管理器，**C++20 + Qt 6 Widgets**，后端调用系统 `7z`（`p7zip-full`）。

## 依赖

```bash
sudo apt install build-essential cmake qt6-base-dev libqt6widgets6 p7zip-full qt6-gtk-platformtheme
```

`qt6-gtk-platformtheme` 让 Qt 在 GNOME 下更接近原生控件外观（推荐）。

## 构建

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
./build/7zip-gui-cpp
```

### 打包 .deb

```bash
./packaging/build-deb.sh
sudo dpkg -i 7zip-gui-cpp_*.deb
sudo apt install -f
```

默认版本号由 `VERSION` 和 `REV` 环境变量控制：

```bash
VERSION=0.1.0 REV=76 ./packaging/build-deb.sh
```

## 用法

```bash
7zip-gui-cpp                      # 空窗口，通过工具栏打开压缩包
7zip-gui-cpp archive.7z           # 打开指定压缩包
7zip-gui-cpp --open archive.7z    # 同上（--open 显式指定）
7zip-gui-cpp --extract-here a.7z  # 解压到压缩包同目录
7zip-gui-cpp --test a.7z          # 测试完整性
7zip-gui-cpp --add file.txt dir/  # 创建压缩包向导
7zip-gui-cpp --lang zh_CN         # 指定语言
```

工具栏操作：

| 按钮          | 功能       |
| ----------- | -------- |
| **Open**    | 选择并打开压缩包 |
| **Extract** | 解压到指定目录  |
| **Test**    | 测试压缩包完整性 |
| **Refresh** | 刷新当前文件列表 |
| **About**   | 关于信息     |

## 项目结构

```
src/
  main.cpp                入口、CLI 参数解析
  MainWindow.cpp/.h       主窗口（工具栏、表格、所有操作流程）
  SevenZipBackend.cpp/.h  7z 命令行封装（list/extract/test/add）
  ArchiveModel.cpp/.h     文件列表 Qt 表格模型
  Worker.cpp/.h           后台 QThread 工作对象
  dialogs/
    AddDialog             创建压缩包参数设置
    ExtractDialog         解压目标目录选择
    PasswordDialog        密码输入
    ProgressDialog        模态进度条
    OperationResultDialog 操作完成统计
  utils/
    ArchiveUtils          路径解析、大小统计、格式化
    AppLocale             语言持久化与切换
    DialogUtils           对话框居中工具
    FallbackZhTranslator  硬编码中文翻译回退
packaging/                打包脚本、.desktop、图标、文件管理器集成
translations/             翻译源文件 (.ts)
```

## 行为说明

| 场景          | 行为                        |
| ----------- | ------------------------- |
| 打开非压缩包文件    | 报错退出                      |
| 打开损坏/不完整压缩包 | 弹错误框，确认后退出                |
| 打开加密压缩包     | 弹密码框，输错可重试，取消退出           |
| 打开大型压缩包     | 5 秒后显示"仍然处理中"提示           |
| 解压完成        | 显示统计（耗时/大小/比率），退出         |
| 测试通过        | 显示"Archive OK."           |
| 测试失败        | 提取关键错误行展示，保存完整日志到 `/tmp/` |

## 右键菜单（文件管理器）

安装 deb 后自动集成：

| 文件管理器                | 方式                                         |
| -------------------- | ------------------------------------------ |
| **Nemo** (Cinnamon)  | `.nemo_action`                             |
| **Caja** (MATE)      | `.caja_action`                             |
| **Nautilus** (GNOME) | `nautilus-python` 扩展（需 `python3-nautilus`） |
| **Thunar** (Xfce)    | UCA 自定义动作                                  |
| **Dolphin** (KDE)    | Service Menu `.desktop`                    |

GNOME 若看不到菜单：`sudo apt install python3-nautilus && nautilus -q`

## 双击设置

```bash
xdg-mime default 7zip-gui-cpp.desktop application/x-7z-compressed
xdg-mime default 7zip-gui-cpp.desktop application/zip
xdg-mime query default application/x-7z-compressed  # 验证
```
