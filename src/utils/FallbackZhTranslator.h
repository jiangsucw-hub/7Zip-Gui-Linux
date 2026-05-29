/** @file FallbackZhTranslator.h — .qm 文件不可用时的硬编码中英翻译回退 */

#pragma once

#include <QHash>
#include <QTranslator>

/**
 * 当 lrelease 未安装、无法构建 .qm 翻译文件时使用
 * 将 Qt 的 tr() 调用对接到硬编码的 QHash 映射
 */
class FallbackZhTranslator : public QTranslator {
public:
    explicit FallbackZhTranslator(QObject *parent = nullptr);

    QString translate(const char *context, const char *sourceText, const char *disambiguation,
                      int n) const override;

private:
    // 外层 key = context（如 "MainWindow"），内层 key = English 原文，value = 中文翻译
    QHash<QByteArray, QHash<QString, QString>> m_map;
};
