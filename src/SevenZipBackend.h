/** @file SevenZipBackend.h — 7z 命令行后端数据结构与接口 */

#pragma once

#include <QString>
#include <QStringList>
#include <QVector>
#include <QMetaType>
#include <functional>

/** 压缩包内的单个条目 */
struct ArchiveEntry {
    QString path;          // 文件/目录路径
    qint64 size = 0;       // 未压缩大小（字节）
    qint64 packedSize = 0; // 压缩后大小（字节）
    QString modified;      // 修改时间字符串
    QString attributes;    // 属性（如 "D" = 目录）
    bool isDir = false;    // 是否为目录
    QString encrypted;     // 加密标记（"+" = 加密）
};

/** 通用 7z 错误 */
class SevenZipError {
public:
    explicit SevenZipError(QString message) : m_message(std::move(message)) {}
    const QString &message() const { return m_message; }

private:
    QString m_message;
};

/** 密码相关错误（需要密码或密码错误） */
class PasswordRequiredError : public SevenZipError {
public:
    explicit PasswordRequiredError(const QString &msg = QStringLiteral("Password required or wrong password"))
        : SevenZipError(msg) {}
};

/**
 * 7z 后端：封装对系统 7z 二进制文件的调用
 * 方法内部 create/destroy QProcess，因此线程安全（每个调用独立栈帧）
 */
class SevenZipBackend {
public:
    SevenZipBackend();

    QString binary() const { return m_binary; }            // 已解析的 7z 二进制路径
    static bool isArchive(const QString &path);            // 根据扩展名判断是否是压缩包

    QVector<ArchiveEntry> listArchive(const QString &path, const QString &password = {});
    bool hasEncryptedEntries(const QString &path);          // 快速检查压缩包是否包含加密条目

    bool extract(const QString &archivePath, const QString &outputDir, const QString &password = {},
                 const QStringList &selectedFiles = {},     // 仅解压选中的文件（空=全部）
                 const std::function<void(int)> &progressCallback = nullptr);

    QString test(const QString &path, const QString &password = {},
                 const std::function<void(int)> &progressCallback = nullptr);

    bool add(const QString &archivePath, const QStringList &files, const QString &password = {},
             const QString &archiveType = QStringLiteral("7z"), int compressionLevel = 5,
             const std::function<void(int)> &progressCallback = nullptr);

private:
    QString m_binary;                                      // 7z 可执行文件路径

    QString find7z() const;                                // 查找系统中的 7z 二进制
    static QString normalizeOutputDir(QString dir);         // 确保路径以 / 结尾
    bool runProcess(QStringList args, const QString &password, QString *combinedOut,
                    const std::function<void(int)> &progressCallback = nullptr);
    QVector<ArchiveEntry> parseSltOutput(const QString &output) const;  // 解析 7z l -slt 输出
    ArchiveEntry entryFromMap(const QMap<QString, QString> &map) const;
    static bool outputNeedsPassword(const QString &combined);          // 检查输出是否提示需要密码
};

// 注册跨线程信号槽使用的类型
Q_DECLARE_METATYPE(ArchiveEntry)
Q_DECLARE_METATYPE(QVector<ArchiveEntry>)
