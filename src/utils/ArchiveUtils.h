/** @file ArchiveUtils.h — 压缩包路径解析与统计工具 */

#pragma once

#include <QMetaType>
#include <QString>
#include <QVector>

#include "SevenZipBackend.h"

/** 解压目标计算结果 */
struct ExtractDestination {
    QString outputDir;          // 目标目录（以 / 结尾）
    bool createdSubfolder;      // 是否创建了子目录
    int topLevelCount = 0;      // 顶级条目数量
};

/** 操作结果统计 */
struct OperationStats {
    qint64 elapsedMs = 0;       // 耗时（毫秒）
    qint64 sizeBefore = 0;      // 操作前大小（字节）
    qint64 sizeAfter = 0;       // 操作后大小（字节）
    bool isCompress = false;    // 是否为压缩操作
};

namespace ArchiveUtils {

    int countTopLevelItems(const QVector<ArchiveEntry> &entries);

    /** 根据压缩包名生成目标目录下的不冲突子文件夹名 */
    QString uniqueSubfolder(const QString &parentDir, const QString &archivePath);

    /** 根据已有条目列表决定解压目标（单条目展开 vs 子目录） */
    ExtractDestination resolveExtractDestinationFromEntries(const QString &baseDir,
                                                            const QString &archivePath,
                                                            const QVector<ArchiveEntry> &entries);

    /** 条目未知时的兜底目标路径 */
    ExtractDestination fallbackExtractDestination(const QString &baseDir, const QString &archivePath);

    qint64 fileOrDirSize(const QString &path);                     // 递归计算文件/文件夹总大小
    qint64 pathsTotalSize(const QStringList &paths);               // 批量计算总大小
    qint64 entriesUncompressedSize(const QVector<ArchiveEntry> &entries);

    QString formatBytes(qint64 bytes);                             // 格式化字节为可读字符串
    QString formatDuration(qint64 ms);                             // 格式化耗时为可读字符串
    QString formatSizeRatio(qint64 before, qint64 after, bool compressing); // 格式化大小比率

} // namespace ArchiveUtils
