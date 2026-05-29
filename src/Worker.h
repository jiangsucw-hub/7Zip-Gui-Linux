/** @file Worker.h — 在独立 QThread 中运行 7z 操作的工作对象 */

#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <functional>
#include <memory>

#include "SevenZipBackend.h"
#include "utils/ArchiveUtils.h"

class Worker : public QObject {
    Q_OBJECT
public:
    explicit Worker(QObject *parent = nullptr);

public slots:
    // 以下槽由主线程通过 QMetaObject::invokeMethod 跨线程调用

    void doProbeForOpen(const QString &path);                      // 打开前探测（判断加密+预列条目）
    void doValidateOpenPassword(const QString &path, const QString &password); // 验证打开密码
    void doList(const QString &path, const QString &password);     // 列出内容
    void doListValidated(const QString &path, const QString &password); // 验证密码后列出
    void doExtract(const QString &archive, const QString &outDir, const QString &password); // GUI 解压
    void doExtractHere(const QString &archive, const QString &baseDir, const QString &password); // CLI 解压
    void doTest(const QString &path, const QString &password);     // 测试完整性
    void doAdd(const QString &archivePath, const QStringList &files, const QString &password,
               const QString &format, int level);                  // 创建压缩包

signals:
    void openStageChanged(QString message);                        // 打开阶段变化
    void openProbeFinished(bool encryptedSignalDetected, QVector<ArchiveEntry> entries);
    void openPasswordValidated();                                  // 密码验证通过
    void progress(int percent);                                    // 操作进度 0-100
    void listFinished(QVector<ArchiveEntry> entries);              // 列出完成
    void extractFinished();                                        // 解压完成
    void extractHereFinished(QString outputDir, qint64 elapsedMs, qint64 sizeBefore, qint64 sizeAfter);
    void testFinished(QString output);                             // 测试完成（输出完整日志）
    void addFinished();                                            // 压缩完成
    void error(QString message, bool passwordError);               // 通用错误信号

private:
    void ensureBackend() {}                                        // 预留的后端检查接口

    std::unique_ptr<SevenZipBackend> m_backend;                    // 7z 后端实例
    std::function<void(int)> m_progress;                           // 进度回调
};
