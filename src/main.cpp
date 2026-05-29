/** @file main.cpp — 程序入口，同时承担 CLI 模式的解压/测试流程 */

#include "MainWindow.h"
#include "SevenZipBackend.h"
#include "Worker.h"
#include "dialogs/ExtractDialog.h"
#include "dialogs/OperationResultDialog.h"
#include "dialogs/PasswordDialog.h"
#include "dialogs/ProgressDialog.h"
#include "utils/DialogUtils.h"
#include "utils/AppLocale.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
#include <QThread>

#include <cstdlib>
#include <memory>
#include <unistd.h>

static constexpr const char kAppDisplayName[] = "7-Zip File Manager";  // 全局显示名称常量

// ─── 环境初始化 ──────────────────────────────────────────────

/** 确保在无 GUI 会话（如 cron）下也有 DISPLAY/XDG_RUNTIME_DIR */
static void ensureGuiSessionEnv()
{
    // 如果没有设置 DISPLAY 也没有 Wayland，回退到 :0
    if (qEnvironmentVariableIsEmpty("DISPLAY") && qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY"))
        qputenv("DISPLAY", ":0");
    // 自动补全 XDG_RUNTIME_DIR（某些桌面环境不会自动设置）
    if (qEnvironmentVariableIsEmpty("XDG_RUNTIME_DIR")) {
        const QByteArray rt = QByteArray("/run/user/") + QByteArray::number(::getuid());
        if (QFileInfo::exists(rt))
            qputenv("XDG_RUNTIME_DIR", rt);
    }
}

// ─── 应用元信息 ──────────────────────────────────────────────

/** 设置 GNOME 友好的应用名称和桌面文件关联 */
static void setupGnomeFriendlyApp(QApplication &app)
{
    app.setApplicationName(QStringLiteral("7zip-gui-cpp"));
    app.setApplicationDisplayName(QObject::tr(kAppDisplayName));
    app.setOrganizationName(QStringLiteral("7zip-gui-cpp"));
    app.setDesktopFileName(QStringLiteral("7zip-gui-cpp"));
}

// ─── CLI 模式辅助 ────────────────────────────────────────────

/** CLI 模式下需要一个不可见的父窗口来承载模态对话框 */
static QWidget *cliParentWidget()
{
    auto *w = new QWidget;
    w->setWindowFlags(Qt::Dialog | Qt::WindowStaysOnTopHint);  // 保持在顶层
    w->setWindowTitle(QObject::tr(kAppDisplayName));
    return w;
}

/** 给弹窗设置置顶标志 */
static void applyStayOnTop(QWidget *widget)
{
    if (!widget)
        return;
    widget->setWindowFlag(Qt::WindowStaysOnTopHint, true);
}

// ─── CLI 消息/结果展示 ───────────────────────────────────────

/** 显示带宽度放大的消息框，用于 CLI 模式的用户交互结果反馈 */
static void showCliMessage(QMessageBox::Icon icon, const QString &title, const QString &text)
{
    constexpr int kDialogWidthScaleNumerator = 13;    // 宽度放大系数分子
    constexpr int kDialogWidthScaleDenominator = 10;  // 宽度放大系数分母

    QMessageBox box(icon, title, text, QMessageBox::Ok);
    const int baseWidth = box.sizeHint().width();          // 获取自然宽度
    if (baseWidth > 0)
        box.setMinimumWidth((baseWidth * kDialogWidthScaleNumerator) / kDialogWidthScaleDenominator);
    applyStayOnTop(&box);
    box.setWindowModality(Qt::ApplicationModal);           // 阻止与其他窗口交互
    DialogUtils::centerPopup(&box);                        // 居中放置
    box.exec();
}

/** 从 7z 的完整测试输出中提取关键错误行作为摘要 */
static QString summarizeTestOutput(const QString &output)
{
    // 如果包含 "Everything is Ok" 则直接返回成功
    const bool ok = output.contains(QStringLiteral("Everything is Ok"), Qt::CaseInsensitive);
    if (ok)
        return QObject::tr("Archive OK.");

    QStringList lines;
    const QStringList all = output.split(QLatin1Char('\n'));
    for (const QString &raw : all) {
        QString line = raw;
        line.remove(QLatin1Char('\r'));   // 去掉回车
        line.remove(QLatin1Char('\b'));   // 去掉退格（7z 进度字符）
        const QString lower = line.toLower();
        // 只保留包含错误/警告关键词的行
        if (lower.contains(QStringLiteral("error")) || lower.contains(QStringLiteral("errors"))
            || lower.contains(QStringLiteral("wrong password"))
            || lower.contains(QStringLiteral("cannot"))
            || lower.contains(QStringLiteral("can not"))
            || lower.contains(QStringLiteral("sub items"))
            || lower.contains(QStringLiteral("archives with errors"))) {
            lines << line.trimmed();
        }
    }
    lines.removeAll(QString());        // 去掉空行
    lines.removeDuplicates();          // 去重
    if (lines.isEmpty())
        return QObject::tr("Test failed. See details in /tmp/7zip-gui-cpp-last-test.txt");
    return lines.mid(0, 8).join(QLatin1Char('\n'));  // 最多显示前 8 行
}

// ─── CLI Worker 会话管理 ─────────────────────────────────────

/** 将 Worker 放入独立线程运行，CLI 模式下需要跨线程调用 */
class CliWorkerSession {
public:
    CliWorkerSession()
    {
        m_worker.moveToThread(&m_thread);  // Worker 对象移动到工作线程
        m_thread.start();                  // 启动工作线程
    }

    ~CliWorkerSession()
    {
        m_thread.quit();       // 请求线程退出
        m_thread.wait(5000);   // 等待最多 5 秒
    }

    Worker *worker() { return &m_worker; }

private:
    QThread m_thread;    // 工作线程（先声明，后于 m_worker 析构）
    Worker m_worker;     // 工作对象
};

// ─── CLI 密码验证 ────────────────────────────────────────────

/** 通过模态进度框验证密码是否正确 */
static bool validatePasswordWithModal(QWidget *parent, CliWorkerSession &session,
                                      const QString &archive, const QString &label,
                                      const QString &password, QString *errorMessage,
                                      bool *passwordError)
{
    ProgressDialog progress(QObject::tr("Validating password…"), parent);
    applyStayOnTop(&progress);
    progress.setLabel(QObject::tr("Validating password for:\n%1").arg(label));
    progress.setIndeterminate(true);  // 验证开始时显示不确定进度条

    QString workerError;
    bool workerPasswordError = false;

    // 连接 Worker 进度信号 → 更新进度条
    QMetaObject::Connection c1 = QObject::connect(
        session.worker(), &Worker::progress, &progress, [&](int percent) {
            progress.setIndeterminate(false);
            progress.setProgress(percent);
        });
    // 连接 Worker 完成信号 → 关闭对话框
    QMetaObject::Connection c2 =
        QObject::connect(session.worker(), &Worker::testFinished, &progress, [&](const QString &) {
            progress.accept();
        });
    // 连接 Worker 错误信号 → 记录错误并关闭对话框
    QMetaObject::Connection c3 = QObject::connect(
        session.worker(), &Worker::error, &progress, [&](const QString &msg, bool pwdErr) {
            workerError = msg;
            workerPasswordError = pwdErr;
            progress.reject();
        });

    // 通过跨线程调用发起测试
    QMetaObject::invokeMethod(session.worker(), "doTest", Qt::QueuedConnection, Q_ARG(QString, archive),
                              Q_ARG(QString, password));
    const int rc = DialogUtils::execCentered(progress, parent);  // 模态等待

    // 清理临时连接
    QObject::disconnect(c1);
    QObject::disconnect(c2);
    QObject::disconnect(c3);

    if (rc == QDialog::Accepted)
        return true;  // 验证通过

    // 验证失败：输出错误信息
    if (errorMessage)
        *errorMessage = workerError;
    if (passwordError)
        *passwordError = workerPasswordError;
    return false;
}

// ─── CLI 测试流程 ────────────────────────────────────────────

/** 通过模态进度框运行完整测试（含密码验证阶段） */
static bool runTestWithModal(QWidget *parent, CliWorkerSession &session, const QString &archive,
                             const QString &password, bool encryptedArchive, QString *testOutput,
                             QString *errorMessage, bool *passwordError)
{
    ProgressDialog progress(QObject::tr("Testing archive…"), parent);
    applyStayOnTop(&progress);
    progress.setIndeterminate(true);
    progress.setLabel(encryptedArchive ? QObject::tr("Validating password...")
                                       : QObject::tr("Testing archive integrity..."));

    QString workerOutput;
    QString workerError;
    bool workerPasswordError = false;
    bool switchedToIntegrityStage = false;  // 标记是否已进入完整性校验阶段

    QMetaObject::Connection c1 = QObject::connect(
        session.worker(), &Worker::progress, &progress, [&](int percent) {
            if (!switchedToIntegrityStage) {
                switchedToIntegrityStage = true;
                progress.setLabel(QObject::tr("Testing archive integrity..."));
                progress.setIndeterminate(false);  // 切换到确定进度
            }
            progress.setProgress(percent);
        });
    QMetaObject::Connection c2 = QObject::connect(
        session.worker(), &Worker::testFinished, &progress, [&](const QString &out) {
            workerOutput = out;
            progress.setLabel(QObject::tr("Preparing result..."));
            progress.setIndeterminate(true);
            QApplication::processEvents();  // 强制刷新 UI
            progress.accept();
        });
    QMetaObject::Connection c3 = QObject::connect(
        session.worker(), &Worker::error, &progress, [&](const QString &msg, bool pwdErr) {
            workerError = msg;
            workerPasswordError = pwdErr;
            progress.reject();
        });

    QMetaObject::invokeMethod(session.worker(), "doTest", Qt::QueuedConnection, Q_ARG(QString, archive),
                              Q_ARG(QString, password));
    const int rc = DialogUtils::execCentered(progress, parent);

    QObject::disconnect(c1);
    QObject::disconnect(c2);
    QObject::disconnect(c3);

    if (rc == QDialog::Accepted) {
        if (testOutput)
            *testOutput = workerOutput;
        return true;
    }

    if (errorMessage)
        *errorMessage = workerError;
    if (passwordError)
        *passwordError = workerPasswordError;
    return false;
}

// ─── CLI 解压到这里 ──────────────────────────────────────────

/** CLI 模式：解压压缩包到指定目录（含密码验证、目标选择、结果展示） */
static int runExtractHere(QWidget *parent, const QString &archive)
{
    if (!QFileInfo::exists(archive)) {
        showCliMessage(QMessageBox::Critical, QObject::tr("Extract failed"),
                       QObject::tr("File does not exist:\n%1").arg(archive));
        return 1;
    }

    const QFileInfo afi(archive);
    SevenZipBackend backend;
    QString password;
    const bool archiveEncrypted = backend.hasEncryptedEntries(archive);  // 探测是否加密
    CliWorkerSession session;

    // 加密压缩包：先验证密码
    if (archiveEncrypted) {
        while (true) {
            PasswordDialog pwdDlg(afi.fileName(), parent);
            applyStayOnTop(&pwdDlg);
            if (DialogUtils::execCentered(pwdDlg, parent) != QDialog::Accepted)
                return 1;
            password = pwdDlg.password();
            if (password.isEmpty())
                return 1;

            QString errorMessage;
            bool passwordError = false;
            const bool validated =
                validatePasswordWithModal(parent, session, archive, afi.fileName(), password,
                                          &errorMessage, &passwordError);
            if (validated)
                break;  // 验证通过，退出密码循环
            if (passwordError) {
                showCliMessage(QMessageBox::Warning, QObject::tr("Wrong password"),
                               QObject::tr("Password is incorrect. Please try again."));
                continue;  // 密码错误，重新尝试
            }
            if (!errorMessage.isEmpty()) {
                showCliMessage(QMessageBox::Critical, QObject::tr("Extract failed"), errorMessage);
            } else {
                showCliMessage(QMessageBox::Critical, QObject::tr("Extract failed"),
                               QObject::tr("Failed to validate archive password."));
            }
            return 1;
        }
    }

    ProgressDialog progress(QObject::tr("Extracting…"), parent);
    applyStayOnTop(&progress);
    progress.setLabel(afi.fileName());
    progress.setProgress(0);

    QString baseDir = afi.absolutePath();  // 默认解压到压缩包同目录
    bool pathChosen = false;               // 是否已选择目标路径

    while (true) {
        // 首次进入或未选择目标路径时弹出目标选择对话框
        if (!pathChosen) {
            ExtractDialog dirDlg(baseDir, parent);
            applyStayOnTop(&dirDlg);
            if (DialogUtils::execCentered(dirDlg, parent) != QDialog::Accepted)
                return 1;
            if (!dirDlg.outputDirectory().trimmed().isEmpty())
                baseDir = dirDlg.outputDirectory().trimmed();
            pathChosen = true;
        }

        OperationStats stats;
        QString outputPath;
        QString errorMessage;
        bool passwordError = false;

        // 进度连接
        QMetaObject::Connection c1 =
            QObject::connect(session.worker(), &Worker::progress, &progress, &ProgressDialog::setProgress);
        // 完成连接：记录统计信息
        QMetaObject::Connection c2 = QObject::connect(
            session.worker(), &Worker::extractHereFinished, &progress,
            [&](const QString &outDir, qint64 elapsedMs, qint64 sizeBefore, qint64 sizeAfter) {
                outputPath = outDir;
                stats.elapsedMs = elapsedMs;
                stats.sizeBefore = sizeBefore;
                stats.sizeAfter = sizeAfter;
                stats.isCompress = false;
                progress.accept();
            });
        // 错误连接
        QMetaObject::Connection c3 = QObject::connect(
            session.worker(), &Worker::error, &progress, [&](const QString &msg, bool pwdErr) {
                errorMessage = msg;
                passwordError = pwdErr;
                progress.reject();
            });

        progress.setProgress(0);
        QMetaObject::invokeMethod(session.worker(), "doExtractHere", Qt::QueuedConnection,
                                  Q_ARG(QString, archive), Q_ARG(QString, baseDir),
                                  Q_ARG(QString, password));
        const int rc = DialogUtils::execCentered(progress, parent);

        QObject::disconnect(c1);
        QObject::disconnect(c2);
        QObject::disconnect(c3);

        if (rc == QDialog::Accepted) {
            progress.close();
            OperationResultDialog dlg(OperationResultDialog::Kind::Extract, stats, outputPath, parent);
            applyStayOnTop(&dlg);
            DialogUtils::execCentered(dlg, parent);
            return 0;
        }

        progress.close();
        if (passwordError) {
            // 密码错误：提示重新输入
            PasswordDialog pwdDlg(afi.fileName(), parent);
            applyStayOnTop(&pwdDlg);
            if (DialogUtils::execCentered(pwdDlg, parent) != QDialog::Accepted)
                return 1;
            password = pwdDlg.password();
            if (password.isEmpty())
                return 1;
            continue;  // 回到循环顶部使用新密码重试
        }

        showCliMessage(QMessageBox::Critical, QObject::tr("Extract failed"), errorMessage);
        return 1;
    }
}

// ─── CLI 测试流程 ────────────────────────────────────────────

/** CLI 模式：测试压缩包完整性（支持加密压缩包的密码重试） */
static int runTest(QWidget *parent, const QString &archive)
{
    if (!QFileInfo::exists(archive)) {
        showCliMessage(QMessageBox::Critical, QObject::tr("Test failed"),
                       QObject::tr("File does not exist:\n%1").arg(archive));
        return 1;
    }

    const QFileInfo afi(archive);
    SevenZipBackend backend;
    // 启动阶段：显示一个短暂的不确定进度框
    ProgressDialog startupProgress(QObject::tr("Testing archive…"), parent);
    applyStayOnTop(&startupProgress);
    startupProgress.setIndeterminate(true);
    startupProgress.setLabel(QObject::tr("Preparing test..."));
    DialogUtils::centerPopup(&startupProgress, parent);
    startupProgress.show();
    QApplication::processEvents();  // 强制渲染启动进度框
    const bool archiveEncrypted = backend.hasEncryptedEntries(archive);
    startupProgress.hide();         // 探测完成，隐藏启动框
    CliWorkerSession session;

    while (true) {
        QString password;
        if (archiveEncrypted) {
            PasswordDialog pwdDlg(afi.fileName(), parent);
            applyStayOnTop(&pwdDlg);
            if (DialogUtils::execCentered(pwdDlg, parent) != QDialog::Accepted
                || pwdDlg.password().isEmpty()) {
                return 0;
            }
            password = pwdDlg.password();
        }

        QString out;
        QString errorMessage;
        bool passwordError = false;
        const bool ok = runTestWithModal(parent, session, archive, password, archiveEncrypted, &out,
                                         &errorMessage, &passwordError);
        if (ok) {
            // 测试通过：输出结果保存到 /tmp 供排查
            QFile file(QStringLiteral("/tmp/7zip-gui-cpp-last-test.txt"));
            if (file.open(QIODevice::WriteOnly | QIODevice::Truncate))
                file.write(out.toUtf8());
            showCliMessage(QMessageBox::Information, QObject::tr("Test"), summarizeTestOutput(out));
            return 0;
        }
        if (passwordError) {
            showCliMessage(QMessageBox::Warning, QObject::tr("Wrong password"),
                           QObject::tr("Password is incorrect. Please try again."));
            continue;  // 密码错误，重新循环
        }
        showCliMessage(QMessageBox::Critical, QObject::tr("Test failed"), errorMessage);
        return 1;
    }
}

// ─── 批量 CLI 处理 ──────────────────────────────────────────

/** 对多个文件依次执行 CLI 操作，返回最后一个非零退出码 */
static int runCliBatch(int (*handler)(QWidget *, const QString &), const QStringList &paths,
                       QWidget *parent)
{
    int lastCode = 0;
    for (const QString &path : paths) {
        const int code = handler(parent, path);
        if (code != 0)
            lastCode = code;  // 记录最后一个错误码
    }
    return lastCode;
}

// ─── main ────────────────────────────────────────────────────

int main(int argc, char *argv[])
{
    // 0. 环境初始化
    ensureGuiSessionEnv();
    QApplication app(argc, argv);
    setupGnomeFriendlyApp(app);

    // 1. 命令行参数定义
    QCommandLineParser parser;
    parser.setApplicationDescription(QObject::tr("Archive manager using 7z"));
    parser.addHelpOption();
    QCommandLineOption extractHere(QStringList{QStringLiteral("extract-here")},
                                   QObject::tr("Extract to folder next to archive"));
    QCommandLineOption testOpt(QStringList{QStringLiteral("test")}, QObject::tr("Test archive"));
    QCommandLineOption extractOpt(QStringList{QStringLiteral("extract")}, QObject::tr("Open extract UI"));
    QCommandLineOption addOpt(QStringList{QStringLiteral("add")}, QObject::tr("Add files to a new archive"));
    QCommandLineOption openOpt(QStringList{QStringLiteral("open")}, QObject::tr("Browse archive contents"));
    QCommandLineOption langOpt(QStringList{QStringLiteral("lang")}, QObject::tr("UI language (en, zh_CN)"),
                             QStringLiteral("code"));
    parser.addOption(extractHere);
    parser.addOption(testOpt);
    parser.addOption(extractOpt);
    parser.addOption(addOpt);
    parser.addOption(openOpt);
    parser.addOption(langOpt);
    parser.addPositionalArgument(QStringLiteral("path"),
                                 QObject::tr("Archive to open, or files for --add"));
    parser.process(app);

    // 2. 语言处理
    if (parser.isSet(langOpt))
        AppLocale::saveLanguage(parser.value(langOpt));
    AppLocale::installForApp(app);

    // 3. CLI 模式 vs GUI 模式路由
    const QStringList pos = parser.positionalArguments();
    const bool cliMode = parser.isSet(extractHere) || parser.isSet(testOpt);
    std::unique_ptr<QWidget> cliParent(cliMode ? cliParentWidget() : nullptr);

    try {
        // 3a. --extract-here
        if (parser.isSet(extractHere)) {
            if (pos.isEmpty()) {
                showCliMessage(QMessageBox::Warning, QObject::tr(kAppDisplayName),
                               QObject::tr("No archive specified."));
                return 1;
            }
            return runCliBatch(runExtractHere, pos, cliParent.get());
        }

        // 3b. --test
        if (parser.isSet(testOpt)) {
            if (pos.isEmpty()) {
                showCliMessage(QMessageBox::Warning, QObject::tr(kAppDisplayName),
                               QObject::tr("No archive specified."));
                return 1;
            }
            return runCliBatch(runTest, pos, cliParent.get());
        }

        // 4. GUI 模式
        MainWindow window;

        // 4a. --add：无头模式创建压缩包后退出
        if (parser.isSet(addOpt)) {
            QStringList files;
            for (const QString &p : pos) {
                if (QFileInfo::exists(p))
                    files << QFileInfo(p).absoluteFilePath();
            }
            window.setHeadlessAddMode(true);
            window.startAddToArchive(files);
            return 0;
        }

        // 4b. --open 或无参数：打开压缩包或显示空窗口
        QString startupArchive;
        if (!pos.isEmpty() && QFileInfo::exists(pos.first()))
            startupArchive = pos.first();
        if (!startupArchive.isEmpty()) {
            window.deferInitialShowUntilOpenCompletes(true);  // 打开完成后再显示窗口
            window.openArchive(startupArchive);
        } else {
            window.show();  // 直接显示空窗口
        }
        return app.exec();  // 进入 Qt 事件循环
    } catch (const SevenZipError &e) {
        showCliMessage(QMessageBox::Critical, QObject::tr(kAppDisplayName), e.message());
        return 1;
    }
}
