/** @file SevenZipBackend.cpp — 调用系统 7z 命令行工具的后端 */

#include "SevenZipBackend.h"

#include <QElapsedTimer>
#include <QFileInfo>
#include <QHash>
#include <QObject>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>

namespace {
constexpr int kProcessPollMs = 500;                     // 进程轮询间隔
constexpr qint64 kProcessTimeoutMs = 24LL * 3600 * 1000; // 24 小时超时
} // namespace

// ─── 构造 / 二进制查找 ───────────────────────────────────────

SevenZipBackend::SevenZipBackend() : m_binary(find7z()) {}

/** 查找系统上可用的 7z 二进制文件 */
QString SevenZipBackend::find7z() const
{
    // 优先用 PATH 查找
    const QStringList names = {QStringLiteral("7z"), QStringLiteral("7zz")};
    for (const QString &name : names) {
        const QString path = QStandardPaths::findExecutable(name);
        if (!path.isEmpty())
            return path;
    }
    // PATH 中找不到，回退到硬编码路径
    for (const QString &p :
         {QStringLiteral("/usr/bin/7z"), QStringLiteral("/usr/bin/7zz"),
          QStringLiteral("/usr/local/bin/7z")}) {
        if (QFileInfo::exists(p))
            return p;
    }
    throw SevenZipError(
        QStringLiteral("7z not found. Install: sudo apt install p7zip-full"));
}

/** 规范化输出目录路径，确保以 '/' 结尾 */
QString SevenZipBackend::normalizeOutputDir(QString dir)
{
    if (dir.isEmpty())
        return dir;
    const QChar last = dir.back();
    if (last != QLatin1Char('/') && last != QLatin1Char('\\'))
        dir += QLatin1Char('/');
    return dir;
}

// ─── 文件类型判断 ────────────────────────────────────────────

/** 根据文件扩展名判断是否是支持的压缩格式 */
bool SevenZipBackend::isArchive(const QString &path)
{
    // 单级扩展名
    static const QStringList exts = {
        QStringLiteral(".7z"),  QStringLiteral(".zip"), QStringLiteral(".rar"),
        QStringLiteral(".tar"), QStringLiteral(".gz"),  QStringLiteral(".bz2"),
        QStringLiteral(".xz"),  QStringLiteral(".tgz"), QStringLiteral(".tbz2"),
        QStringLiteral(".txz"), QStringLiteral(".iso"), QStringLiteral(".wim"),
        QStringLiteral(".cab"), QStringLiteral(".deb"), QStringLiteral(".rpm"),
        QStringLiteral(".zst"), QStringLiteral(".lz4"),
    };
    const QString lower = path.toLower();
    for (const QString &ext : exts) {
        if (lower.endsWith(ext))
            return true;
    }
    // 复合扩展名（如 .tar.gz）
    const QStringList compound = {QStringLiteral(".tar.gz"), QStringLiteral(".tar.bz2"),
                                  QStringLiteral(".tar.xz")};
    for (const QString &ext : compound) {
        if (lower.endsWith(ext))
            return true;
    }
    return false;
}

// ─── 输出分析 ────────────────────────────────────────────────

/** 检查 7z 输出是否提示需要密码 */
bool SevenZipBackend::outputNeedsPassword(const QString &combined)
{
    const QString lower = combined.toLower();
    static const QStringList markers = {
        QStringLiteral("wrong password"),
        QStringLiteral("enter password"),
        QStringLiteral("can not open encrypted archive"),
        QStringLiteral("cannot open encrypted archive"),
        QStringLiteral("data error in encrypted"),
        QStringLiteral("encrypted file"),
        QStringLiteral("need password"),
    };
    for (const QString &m : markers) {
        if (lower.contains(m))
            return true;
    }
    return false;
}

// ─── 进程运行核心 ────────────────────────────────────────────

/** 启动 7z 子进程并等待完成，收集输出并回调进度 */
bool SevenZipBackend::runProcess(QStringList args, const QString &password, QString *combinedOut,
                                 const std::function<void(int)> &progressCallback)
{
    // 强制 UTF-8 输出以避免 GBK/其他编码导致的乱码
    args.prepend(QStringLiteral("-sccUTF-8"));
    // 如果有密码，使用 -p 让 7z 从 stdin 读取，避免密码出现在 /proc/.../cmdline
    if (!password.isEmpty())
        args.prepend(QStringLiteral("-p"));
    // 如果需要进度回调，启用 7z 进度输出到 stderr 的模式
    if (progressCallback)
        args.append(QStringLiteral("-bsp1"));

    QProcess proc;
    proc.setProgram(m_binary);
    proc.setArguments(args);

    QByteArray allOut;               // 收集全部输出
    int lastPercent = -1;            // 上次报告的百分比（避免重复回调）
    bool passwordPromptSeen = false; // 是否检测到密码提示

    // 消费输出块：追加到缓存，检测密码提示，提取进度百分比
    auto consumeChunk = [&](const QByteArray &chunk) {
        if (chunk.isEmpty())
            return;
        allOut += chunk;
        // 密码为空时检查是否需要密码（防止子进程阻塞等待）
        if (password.isEmpty()) {
            const QString lower = QString::fromUtf8(chunk).toLower();
            if (lower.contains(QStringLiteral("enter password"))) {
                passwordPromptSeen = true;
                if (proc.state() != QProcess::NotRunning)
                    proc.kill();  // 立即终止以避免挂起
            }
        }
        if (!progressCallback)
            return;
        // 从输出中解析 "XX%" 形式的进度
        static const QRegularExpression kProgressRe(QStringLiteral("(\\d+)%"));
        const QString text = QString::fromUtf8(chunk);
        auto it = kProgressRe.globalMatch(text);
        while (it.hasNext()) {
            const int pct = it.next().captured(1).toInt();
            if (pct > lastPercent) {
                lastPercent = pct;
                progressCallback(pct);
            }
        }
    };

    // 连接 stdout 信号
    QObject::connect(&proc, &QProcess::readyReadStandardOutput, [&]() {
        consumeChunk(proc.readAllStandardOutput());
    });
    // 连接 stderr 信号（7z 进度信息输出到 stderr）
    QObject::connect(&proc, &QProcess::readyReadStandardError, [&]() {
        consumeChunk(proc.readAllStandardError());
    });

    proc.start();
    if (!proc.waitForStarted(30000)) {
        proc.kill();
        throw SevenZipError(QStringLiteral("7z process failed to start"));
    }

    // 通过 stdin 传递密码，避免密码出现在 /proc/PID/cmdline 中
    if (!password.isEmpty()) {
        proc.write((password + QLatin1Char('\n')).toUtf8());
        proc.closeWriteChannel();
    }

    // 轮询等待进程结束或超时
    QElapsedTimer timer;
    timer.start();
    while (!proc.waitForFinished(kProcessPollMs)) {
        if (timer.elapsed() > kProcessTimeoutMs) {
            proc.kill();
            proc.waitForFinished(5000);
            throw SevenZipError(QStringLiteral("7z process timed out"));
        }
    }

    // 进程结束后再吃一次剩余输出
    consumeChunk(proc.readAllStandardOutput());
    consumeChunk(proc.readAllStandardError());

    if (combinedOut)
        *combinedOut = QString::fromUtf8(allOut);

    // 密码提示检测到 = 需要密码
    if (passwordPromptSeen)
        return false;
    // 正常退出且返回码为 0 表示成功
    return proc.exitStatus() == QProcess::NormalExit && proc.exitCode() == 0;
}

// ─── 列出压缩包内容 ──────────────────────────────────────────

/** 列出压缩包内所有条目（-slt 格式便于解析） */
QVector<ArchiveEntry> SevenZipBackend::listArchive(const QString &path, const QString &password)
{
    QStringList args = {QStringLiteral("l"), QStringLiteral("-slt"), path};
    QString combined;
    const bool ok = runProcess(args, password, &combined);
    if (!ok) {
        if (outputNeedsPassword(combined))
            throw PasswordRequiredError();
        throw SevenZipError(QStringLiteral("Failed to list archive:\n%1").arg(combined.trimmed()));
    }
    return parseSltOutput(combined);
}

/** 解析 7z l -slt 格式的输出为 ArchiveEntry 列表 */
QVector<ArchiveEntry> SevenZipBackend::parseSltOutput(const QString &output) const
{
    QVector<ArchiveEntry> entries;
    QHash<QString, QString> current;  // 当前条目的属性集合
    bool pastHeader = false;         // 是否已跳过表头分割线

    const QStringList lines = output.split(QLatin1Char('\n'));

    // 当遇到空行或新的 Path 键时，将累积的 current 刷新为一个条目
    auto flush = [&]() {
        if (current.contains(QStringLiteral("Path")))
            entries.append(entryFromMap(current));
        current.clear();
    };

    for (QString line : lines) {
        line = line.trimmed();
        // 跳过表头
        if (!pastHeader) {
            if (line.startsWith(QStringLiteral("----------"))
                || line.startsWith(QStringLiteral("==========")))
                pastHeader = true;
            continue;
        }
        // 空行 = 条目分隔
        if (line.isEmpty()) {
            flush();
            continue;
        }
        // 解析 "Key = Value" 格式
        const int eq = line.indexOf(QLatin1Char('='));
        if (eq <= 0)
            continue;
        const QString key = line.left(eq).trimmed();
        const QString value = line.mid(eq + 1).trimmed();
        // 遇到一个新的 Path 键表示上一个条目结束
        if (key == QStringLiteral("Path") && current.contains(QStringLiteral("Path"))) {
            entries.append(entryFromMap(current));
            current.clear();
        }
        current[key] = value;
    }
    // 处理最后一个条目
    if (current.contains(QStringLiteral("Path")))
        entries.append(entryFromMap(current));
    return entries;
}

/** 从 key-value map 构造一个 ArchiveEntry */
ArchiveEntry SevenZipBackend::entryFromMap(const QHash<QString, QString> &d) const
{
    ArchiveEntry e;
    e.path = d.value(QStringLiteral("Path"));
    e.size = d.value(QStringLiteral("Size")).toLongLong();
    e.packedSize = d.value(QStringLiteral("Packed Size")).toLongLong();
    e.modified = d.value(QStringLiteral("Modified"));
    e.attributes = d.value(QStringLiteral("Attributes"));
    e.encrypted = d.value(QStringLiteral("Encrypted"));
    e.isDir = e.attributes.startsWith(QLatin1Char('D'));
    return e;
}

// ─── 加密探测 ────────────────────────────────────────────────

/** 快速检查压缩包是否包含加密条目（不带密码列出，从输出特征判断） */
bool SevenZipBackend::hasEncryptedEntries(const QString &path)
{
    try {
        const auto entries = listArchive(path);  // 不带密码尝试列出
        for (const ArchiveEntry &e : entries) {
            if (e.encrypted == QLatin1Char('+'))
                return true;
        }
        return false;
    } catch (const PasswordRequiredError &) {
        return true;  // 列出即报密码错误，说明加密
    }
}

// ─── 解压 ────────────────────────────────────────────────────

/** 解压压缩包到指定目录 */
bool SevenZipBackend::extract(const QString &archivePath, const QString &outputDir,
                              const QString &password, const QStringList &selectedFiles,
                              const std::function<void(int)> &progressCallback)
{
    const QString outDir = normalizeOutputDir(outputDir);
    QStringList args = {QStringLiteral("x"), archivePath,
                        QStringLiteral("-o") + outDir, QStringLiteral("-y")};  // -y 自动确认
    args.append(selectedFiles);  // 如果指定了文件列表，只解压这些文件

    QString combined;
    const bool ok = runProcess(args, password, &combined, progressCallback);
    if (!ok) {
        if (outputNeedsPassword(combined))
            throw PasswordRequiredError();
        throw SevenZipError(QStringLiteral("Extraction failed:\n%1").arg(combined.trimmed()));
    }
    return true;
}

// ─── 测试 ────────────────────────────────────────────────────

/** 测试压缩包完整性，返回 7z 的完整输出 */
QString SevenZipBackend::test(const QString &path, const QString &password,
                              const std::function<void(int)> &progressCallback)
{
    QStringList args = {QStringLiteral("t"), path};
    QString combined;
    const bool ok = runProcess(args, password, &combined, progressCallback);
    if (!ok) {
        if (outputNeedsPassword(combined))
            throw PasswordRequiredError();
        throw SevenZipError(QStringLiteral("Test failed:\n%1").arg(combined.trimmed()));
    }
    return combined;
}

// ─── 创建压缩包 ──────────────────────────────────────────────

/** 创建压缩包 */
bool SevenZipBackend::add(const QString &archivePath, const QStringList &files,
                          const QString &password, const QString &archiveType, int compressionLevel,
                          const std::function<void(int)> &progressCallback)
{
    QStringList args = {QStringLiteral("a"), archivePath, QStringLiteral("-y")};
    args.append(QStringLiteral("-mmt=on"));                          // 多线程压缩
    args.append(files);                                               // 要添加的文件列表
    if (!archiveType.isEmpty())
        args.append(QStringLiteral("-t") + archiveType);             // 压缩格式
    args.append(QStringLiteral("-mx") + QString::number(compressionLevel)); // 压缩级别

    QString combined;
    const bool ok = runProcess(args, password, &combined, progressCallback);
    if (!ok) {
        if (outputNeedsPassword(combined))
            throw PasswordRequiredError();
        throw SevenZipError(QStringLiteral("Failed to create archive:\n%1").arg(combined.trimmed()));
    }
    return true;
}
