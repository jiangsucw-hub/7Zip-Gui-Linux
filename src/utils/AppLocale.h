/** @file AppLocale.h — 应用语言持久化与运行时切换 */

#pragma once

#include <QString>

class QApplication;
class QWidget;

namespace AppLocale {

    /** 从 QSettings 读取上次保存的语言代码 */
    QString savedLanguage();

    /** 保存语言代码到 QSettings */
    void saveLanguage(const QString &code);

    /** 启动时安装翻译器到 app */
    void installForApp(QApplication &app);

    /** 安装/卸载对应语言的翻译器；retranslateRoot 接收 LanguageChange 事件 */
    void applyLanguage(const QString &code, QWidget *retranslateRoot = nullptr);

    /** 返回语言代码的可读名称 */
    QString languageDisplayName(const QString &code);

} // namespace AppLocale
