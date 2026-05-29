/** @file FallbackZhTranslator.cpp — 硬编码中英文翻译回退 */

#include "FallbackZhTranslator.h"

#include <QByteArray>

FallbackZhTranslator::FallbackZhTranslator(QObject *parent) : QTranslator(parent)
{
    // 使用 builder lambda 添加翻译条目的便捷方式
    auto add = [this](const char *ctx, const char *en, const char *zh) {
        m_map[QByteArray(ctx)].insert(QString::fromUtf8(en), QString::fromUtf8(zh));
    };

    // ── MainWindow 菜单和工具栏 ──
    add("MainWindow", "7-Zip File Manager", "7-Zip 文件管理器");
    add("MainWindow", "Archives", "压缩包");
    add("MainWindow", "Open an archive to begin", "请打开压缩包");
    add("MainWindow", "No archive open", "未打开压缩包");
    add("MainWindow", "&File", "文件(&F)");
    add("MainWindow", "&Archive", "压缩包(&A)");
    add("MainWindow", "&Language", "语言(&L)");
    add("MainWindow", "&Help", "帮助(&H)");
    add("MainWindow", "&Open…", "打开…");
    add("MainWindow", "&Add to archive…", "添加到压缩包…");
    add("MainWindow", "&Extract…", "解压…");
    add("MainWindow", "&Test", "测试");
    add("MainWindow", "&Refresh", "刷新");
    add("MainWindow", "Open…", "打开…");
    add("MainWindow", "Open...", "打开...");
    add("MainWindow", "Extract…", "解压…");
    add("MainWindow", "Extract...", "解压...");
    add("MainWindow", "Test", "测试");
    add("MainWindow", "Refresh", "刷新");
    add("MainWindow", "About", "关于");
    add("MainWindow", "Extract", "解压");
    add("MainWindow", "Extraction complete", "解压完成");
    add("MainWindow", "Archive created", "压缩包已创建");
    add("MainWindow", "Finalizing archive…", "正在收尾…");
    add("MainWindow", "English", "English");
    add("MainWindow", "简体中文", "简体中文");

    // ── 操作结果对话框 ──
    add("OperationResultDialog", "Extraction complete", "解压完成");
    add("OperationResultDialog", "Archive created", "压缩包已创建");
    add("OperationResultDialog", "Time elapsed:", "耗时：");
    add("OperationResultDialog", "Archive size:", "压缩包大小：");
    add("OperationResultDialog", "Extracted size:", "解压后大小：");
    add("OperationResultDialog", "Uncompressed size:", "压缩前大小：");
    add("OperationResultDialog", "Compressed size:", "压缩后大小：");
    add("OperationResultDialog", "Size ratio:", "大小比率：");

    // ── 全局 ──
    add("QApplication", "7-Zip File Manager", "7-Zip 文件管理器");
    add("QApplication", "Archives", "压缩包");
}

QString FallbackZhTranslator::translate(const char *context, const char *sourceText,
                                        const char *disambiguation, int n) const
{
    Q_UNUSED(disambiguation);
    Q_UNUSED(n);
    const auto ctx = m_map.find(QByteArray(context));                 // 按上下文查找
    if (ctx == m_map.end())
        return {};
    const auto it = ctx->find(QString::fromUtf8(sourceText));         // 按原文查找翻译
    if (it == ctx->end())
        return {};
    return *it;
}
