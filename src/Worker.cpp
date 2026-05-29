/** @file Worker.cpp — 在独立 QThread 中执行 7z 操作的工作对象 */

#include "Worker.h"

#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>

Worker::Worker(QObject *parent) : QObject(parent), m_backend(std::make_unique<SevenZipBackend>())
{
    // 进度回调绑定到 signal 发射
    m_progress = [this](int p) { emit progress(p); };
}

// ─── 打开（探测阶段）─────────────────────────────────────────

/** 不带密码探测压缩包：获取条目列表并判断是否加密 */
void Worker::doProbeForOpen(const QString &path)
{
    ensureBackend();
    try {
        emit openStageChanged(tr("Reading archive header..."));
        bool encryptedSignalDetected = false;
        QVector<ArchiveEntry> probeEntries;

        try {
            emit openStageChanged(tr("Loading file list..."));
            probeEntries = m_backend->listArchive(path);          // 尝试不带密码列出
            for (const ArchiveEntry &entry : probeEntries) {
                if (entry.encrypted == QLatin1Char('+')) {        // 检查加密标记
                    encryptedSignalDetected = true;
                    break;
                }
            }
        } catch (const PasswordRequiredError &) {
            encryptedSignalDetected = true;                       // 列出即报密码错 = 加密
        }

        // 仅当 listing 未返回任何条目时才用 test 做二次探测
        // （某些格式可能整体加密而不暴露每个条目的加密标记）
        if (!encryptedSignalDetected && probeEntries.isEmpty()) {
            try {
                m_backend->test(path);                            // 不带密码测试
            } catch (const PasswordRequiredError &) {
                encryptedSignalDetected = true;
            }
        }

        emit openProbeFinished(encryptedSignalDetected, std::move(probeEntries));
    } catch (const SevenZipError &e) {
        emit error(e.message(), false);
    }
}

// ─── 密码验证 ────────────────────────────────────────────────

/** 验证打开加密压缩包时输入的密码是否正确 */
void Worker::doValidateOpenPassword(const QString &path, const QString &password)
{
    ensureBackend();
    try {
        if (password.isEmpty())
            throw PasswordRequiredError();
        emit openStageChanged(tr("Validating password..."));
        m_backend->test(path, password);                           // 用 test 严格验证密码
        emit openStageChanged(tr("Loading file list..."));
        emit openPasswordValidated();
    } catch (const PasswordRequiredError &) {
        emit error(tr("Password required or incorrect."), true);
    } catch (const SevenZipError &e) {
        emit error(e.message(), false);
    }
}

// ─── 列出 ────────────────────────────────────────────────────

/** 列出压缩包内容（非加密或密码已验证时使用） */
void Worker::doList(const QString &path, const QString &password)
{
    ensureBackend();
    try {
        emit openStageChanged(tr("Loading file list..."));
        emit listFinished(m_backend->listArchive(path, password));
    } catch (const PasswordRequiredError &) {
        emit error(tr("Password required or incorrect."), true);
    } catch (const SevenZipError &e) {
        emit error(e.message(), false);
    }
}

/** 带密码验证的列出（先 test 验证密码，再 list 获取内容） */
void Worker::doListValidated(const QString &path, const QString &password)
{
    ensureBackend();
    try {
        if (password.isEmpty())
            throw PasswordRequiredError();
        emit openStageChanged(tr("Validating password..."));
        m_backend->test(path, password);                           // 先严格验证
        emit openStageChanged(tr("Opening archive..."));
        emit openStageChanged(tr("Loading file list..."));
        emit listFinished(m_backend->listArchive(path, password)); // 再列出
    } catch (const PasswordRequiredError &) {
        emit error(tr("Password required or incorrect."), true);
    } catch (const SevenZipError &e) {
        emit error(e.message(), false);
    }
}

// ─── 解压 ────────────────────────────────────────────────────

/** 解压压缩包（从 GUI 的 Extract 按钮触发） */
void Worker::doExtract(const QString &archive, const QString &outDir, const QString &password)
{
    ensureBackend();
    try {
        m_backend->extract(archive, outDir, password, {}, m_progress);
        emit extractFinished();
    } catch (const PasswordRequiredError &) {
        emit error(tr("Password required or incorrect."), true);
    } catch (const SevenZipError &e) {
        emit error(e.message(), false);
    }
}

/** 解压到这里（CLI --extract-here 或文件管理器右键菜单触发），含统计信息 */
void Worker::doExtractHere(const QString &archive, const QString &baseDir, const QString &password)
{
    ensureBackend();
    // 总是创建子目录解压，避免需要预先列出压缩包内容（消除双重读取）
    const QString outputDir = ArchiveUtils::uniqueSubfolder(baseDir, archive) + QLatin1Char('/');
    const bool outputDirExistedBefore = QDir(outputDir).exists();
    try {
        QElapsedTimer timer;
        timer.start();
        const qint64 sizeBefore = QFileInfo(archive).size();

        m_backend->extract(archive, outputDir, password, {}, m_progress);

        // 从文件系统计算未压缩大小，避免二次读取压缩包
        const qint64 uncompressedSize = ArchiveUtils::fileOrDirSize(outputDir);

        emit extractHereFinished(outputDir, timer.elapsed(), sizeBefore, uncompressedSize);
    } catch (const PasswordRequiredError &) {
        if (!outputDir.isEmpty() && !outputDirExistedBefore)
            QDir(outputDir).removeRecursively();
        emit error(tr("Password required or incorrect."), true);
    } catch (const SevenZipError &e) {
        if (!outputDir.isEmpty() && !outputDirExistedBefore)
            QDir(outputDir).removeRecursively();
        emit error(e.message(), false);
    }
}

// ─── 测试 ────────────────────────────────────────────────────

/** 测试压缩包完整性 */
void Worker::doTest(const QString &path, const QString &password)
{
    ensureBackend();
    try {
        const QString out = m_backend->test(path, password, m_progress);
        emit testFinished(out);
    } catch (const PasswordRequiredError &) {
        emit error(tr("Password required or incorrect."), true);
    } catch (const SevenZipError &e) {
        emit error(e.message(), false);
    }
}

// ─── 创建压缩包 ──────────────────────────────────────────────

/** 创建压缩包 */
void Worker::doAdd(const QString &archivePath, const QStringList &files, const QString &password,
                   const QString &format, int level)
{
    ensureBackend();
    try {
        m_backend->add(archivePath, files, password, format, level, m_progress);
        emit addFinished();
    } catch (const PasswordRequiredError &) {
        emit error(tr("Password required or incorrect."), true);
    } catch (const SevenZipError &e) {
        emit error(e.message(), false);
    }
}
