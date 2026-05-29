/** @file ArchiveUtils.cpp — 压缩包相关的工具函数 */

#include "ArchiveUtils.h"

#include <QDir>
#include <QFileInfo>
#include <QLocale>
#include <QSet>

namespace ArchiveUtils {

// ─── 目录结构分析 ────────────────────────────────────────────

/** 如果压缩包根级别只有一个目录，返回该目录名，否则返回空 */
static QString singleTopLevelName(const QVector<ArchiveEntry> &entries)
{
    QSet<QString> roots;
    for (const ArchiveEntry &e : entries) {
        QString p = e.path;
        if (p.endsWith(QLatin1Char('/')))
            p.chop(1);                                    // 去掉尾部斜杠
        if (p.isEmpty())
            continue;
        const int slash = p.indexOf(QLatin1Char('/'));    // 取第一级目录/文件名
        const QString root = slash < 0 ? p : p.left(slash);
        if (!root.isEmpty())
            roots.insert(root);
    }
    if (roots.size() != 1)
        return {};                                        // 多于一个顶级条目
    return *roots.begin();
}

/** 统计压缩包中顶级条目的数量 */
int countTopLevelItems(const QVector<ArchiveEntry> &entries)
{
    QSet<QString> roots;
    for (const ArchiveEntry &e : entries) {
        QString p = e.path;
        if (p.endsWith(QLatin1Char('/')))
            p.chop(1);
        if (p.isEmpty())
            continue;
        const int slash = p.indexOf(QLatin1Char('/'));
        const QString root = slash < 0 ? p : p.left(slash);
        if (!root.isEmpty())
            roots.insert(root);
    }
    return roots.size();
}

// ─── 解压目标目录 ────────────────────────────────────────────

/**
 * 根据压缩包名生成唯一的子目录名
 * 例如 parentDir=/tmp archive.7z → /tmp/archive/
 * 如果已存在 → /tmp/archive_1/ → /tmp/archive_2/ ...
 */
QString uniqueSubfolder(const QString &parentDir, const QString &archivePath)
{
    const QFileInfo fi(archivePath);
    const QString base = fi.completeBaseName();             // 不含扩展名的文件名
    QString parent = parentDir;
    if (!parent.endsWith(QLatin1Char('/')))
        parent += QLatin1Char('/');
    QString out = parent + base;
    int n = 1;
    while (QDir(out).exists()) {                            // 递增编号直到不冲突
        out = parent + base + QLatin1Char('_') + QString::number(n);
        ++n;
    }
    return out;
}

/** 根据条目列表决定解压目标：单条目展开到父目录，多条目的创建子目录 */
ExtractDestination resolveExtractDestinationFromEntries(const QString &baseDir,
                                                        const QString &archivePath,
                                                        const QVector<ArchiveEntry> &entries)
{
    ExtractDestination dest;
    dest.topLevelCount = countTopLevelItems(entries);       // 统计顶级条目数

    QString parent = baseDir;
    if (parent.isEmpty())
        parent = QFileInfo(archivePath).absolutePath();
    if (!parent.endsWith(QLatin1Char('/')))
        parent += QLatin1Char('/');

    if (dest.topLevelCount <= 1) {
        // 单条目：直接解压到父目录（除非冲突则创建子目录）
        const QString rootName = singleTopLevelName(entries);
        const bool wouldConflict =
            !rootName.isEmpty() && QFileInfo::exists(parent + rootName);
        if (wouldConflict) {
            dest.outputDir = uniqueSubfolder(parent, archivePath) + QLatin1Char('/');
            dest.createdSubfolder = true;
        } else {
            dest.outputDir = parent;
            dest.createdSubfolder = false;
        }
    } else {
        // 多条目：总是创建子目录
        dest.outputDir = uniqueSubfolder(parent, archivePath) + QLatin1Char('/');
        dest.createdSubfolder = true;
    }
    return dest;
}

/** 当条目未知时（如未输入密码）的兜底目标路径 */
ExtractDestination fallbackExtractDestination(const QString &baseDir, const QString &archivePath)
{
    ExtractDestination dest;
    dest.topLevelCount = 2;                                 // 保守假设多条目
    dest.createdSubfolder = true;

    QString parent = baseDir;
    if (parent.isEmpty())
        parent = QFileInfo(archivePath).absolutePath();
    if (!parent.endsWith(QLatin1Char('/')))
        parent += QLatin1Char('/');
    dest.outputDir = uniqueSubfolder(parent, archivePath) + QLatin1Char('/');
    return dest;
}

// ─── 文件大小计算 ────────────────────────────────────────────

/** 递归计算文件/目录的总大小 */
qint64 fileOrDirSize(const QString &path)
{
    const QFileInfo fi(path);
    if (!fi.exists())
        return 0;
    if (fi.isFile())
        return fi.size();

    qint64 total = 0;
    const QDir dir(path);
    const QFileInfoList children =
        dir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden);
    for (const QFileInfo &child : children) {
        if (child.isDir())
            total += fileOrDirSize(child.absoluteFilePath());  // 递归
        else
            total += child.size();
    }
    return total;
}

/** 计算多个路径的总大小（用于压缩前统计） */
qint64 pathsTotalSize(const QStringList &paths)
{
    qint64 total = 0;
    for (const QString &p : paths)
        total += fileOrDirSize(p);
    return total;
}

/** 计算压缩包内所有条目的未压缩总大小 */
qint64 entriesUncompressedSize(const QVector<ArchiveEntry> &entries)
{
    qint64 total = 0;
    for (const ArchiveEntry &e : entries) {
        if (!e.isDir)
            total += e.size;                                // 目录不计大小
    }
    return total;
}

// ─── 格式化 ──────────────────────────────────────────────────

QString formatBytes(qint64 bytes)
{
    static const QLocale locale;
    if (bytes < 0)
        return QStringLiteral("—");
    if (bytes < 1024)
        return QString::number(bytes) + QStringLiteral(" B");
    if (bytes < 1024LL * 1024)
        return locale.toString(bytes / 1024.0, 'f', 1) + QStringLiteral(" KB");
    if (bytes < 1024LL * 1024 * 1024)
        return locale.toString(bytes / (1024.0 * 1024.0), 'f', 2) + QStringLiteral(" MB");
    return locale.toString(bytes / (1024.0 * 1024.0 * 1024.0), 'f', 2) + QStringLiteral(" GB");
}

QString formatDuration(qint64 ms)
{
    static const QLocale locale;
    if (ms < 1000)
        return locale.toString(ms / 1000.0, 'f', 2) + QStringLiteral(" s");
    const qint64 sec = ms / 1000;
    if (sec < 60)
        return locale.toString(sec) + QStringLiteral(" s");
    return locale.toString(sec / 60) + QStringLiteral(" min ")
           + locale.toString(sec % 60) + QStringLiteral(" s");
}

QString formatSizeRatio(qint64 before, qint64 after, bool compressing)
{
    static const QLocale locale;
    if (before <= 0 || after < 0)
        return QStringLiteral("—");
    const double pct = 100.0 * static_cast<double>(after) / static_cast<double>(before);
    return locale.toString(pct, 'f', 1) + QStringLiteral("%");
}

} // namespace ArchiveUtils
