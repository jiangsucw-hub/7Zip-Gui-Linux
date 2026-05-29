/** @file AppLocale.cpp — 应用语言管理与翻译安装 */

#include "AppLocale.h"
#include "FallbackZhTranslator.h"

#include <QApplication>
#include <QEvent>
#include <QWidget>
#include <QSettings>
#include <QTranslator>

namespace {

/** 全局 Qt 翻译器（加载 .qm 文件） */
QTranslator &appTranslator()
{
    static QTranslator tr;
    return tr;
}

/** 硬编码回退翻译器（当 .qm 文件不存在时使用） */
FallbackZhTranslator &fallbackZhTranslator()
{
    static FallbackZhTranslator tr;
    return tr;
}

/** 标准化语言代码：zh_* 系列 → zh_CN，其余 → en */
QString normalizeCode(const QString &code)
{
    if (code.startsWith(QLatin1String("zh"), Qt::CaseInsensitive))
        return QStringLiteral("zh_CN");
    return QStringLiteral("en");
}

} // namespace

namespace AppLocale {

/** 从 QSettings 读取保存的语言偏好 */
QString savedLanguage()
{
    return normalizeCode(
        QSettings(QStringLiteral("7zip-gui-cpp"), QStringLiteral("7zip-gui-cpp"))
            .value(QStringLiteral("ui/language"), QStringLiteral("en"))
            .toString());
}

/** 保存语言偏好到 QSettings */
void saveLanguage(const QString &code)
{
    QSettings s(QStringLiteral("7zip-gui-cpp"), QStringLiteral("7zip-gui-cpp"));
    s.setValue(QStringLiteral("ui/language"), normalizeCode(code));
    s.sync();                                                        // 立即写入磁盘
}

/** 应用启动时安装翻译 */
void installForApp(QApplication &app)
{
    applyLanguage(savedLanguage(), nullptr);
    Q_UNUSED(app);
}

/**
 * 应用翻译到应用程序
 * retranslateRoot 不为空时发送 LanguageChange 事件触发 UI 重绘
 */
void applyLanguage(const QString &code, QWidget *retranslateRoot)
{
    const QString norm = normalizeCode(code);
    QApplication *app = qobject_cast<QApplication *>(QCoreApplication::instance());
    if (!app)
        return;

    // 先移除所有翻译
    app->removeTranslator(&appTranslator());
    app->removeTranslator(&fallbackZhTranslator());

    if (norm == QStringLiteral("zh_CN")) {
        // 尝试加载 .qm 翻译文件（优先从资源，再尝试路径）
        const bool loaded = appTranslator().load(QStringLiteral("7zip-gui-cpp_zh_CN"),
                                                   QStringLiteral(":/i18n"))
                              || appTranslator().load(QStringLiteral(":/i18n/7zip-gui-cpp_zh_CN.qm"));
        if (loaded)
            app->installTranslator(&appTranslator());                 // 使用 .qm 翻译
        else
            app->installTranslator(&fallbackZhTranslator());          // 回退到硬编码翻译
    }

    // 发送 LanguageChange 事件让所有控件 retranslate
    if (retranslateRoot) {
        QEvent ev(QEvent::LanguageChange);
        QApplication::sendEvent(static_cast<QObject *>(retranslateRoot), &ev);
    }
}

QString languageDisplayName(const QString &code)
{
    return normalizeCode(code) == QStringLiteral("zh_CN") ? QStringLiteral("简体中文")
                                                          : QStringLiteral("English");
}

} // namespace AppLocale
