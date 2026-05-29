/** @file MainWindow.cpp — 主窗口，包含 GUI 和所有操作流程（Open/Extract/Test/Add） */

#include "MainWindow.h"

#include "dialogs/AddDialog.h"
#include "dialogs/ExtractDialog.h"
#include "dialogs/PasswordDialog.h"
#include "dialogs/OperationResultDialog.h"
#include "dialogs/ProgressDialog.h"
#include "utils/AppLocale.h"
#include "utils/ArchiveUtils.h"
#include "utils/DialogUtils.h"

#include <QDir>
#include <QEvent>
#include <QStandardPaths>

#include <QAction>
#include <QComboBox>
#include <QCoreApplication>
#include <QFileDialog>
#include <QFileInfo>
#include <QFile>
#include <QHeaderView>
#include <QGuiApplication>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QProgressDialog>
#include <QScreen>
#include <QSignalBlocker>
#include <QStatusBar>
#include <QTableView>
#include <QThread>
#include <QToolBar>
#include <QSizePolicy>
#include <QtConcurrent/QtConcurrentRun>
#include <QVBoxLayout>
#include <QWidget>

constexpr int kOpenSlowHintMs = 5000;  // 打开大型压缩包时显示"仍然在加载"提示的延迟

// ─── 图标加载辅助函数 ────────────────────────────────────────

/** 从系统主题获取图标 */
static QIcon themeIcon(const char *name)
{
    return QIcon::fromTheme(QLatin1String(name));
}

/** 加载 About 对话框图标，依次尝试多个候选路径 */
static QIcon aboutDialogIcon()
{
    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
        QDir(appDir).absoluteFilePath(                             // 开发时的相对路径
            QStringLiteral("../packaging/icons/icons-480.png")),
        QDir(appDir).absoluteFilePath(
            QStringLiteral("../packaging/icons/icons8-7-zip-480-whitebg-preview.png")),
        QDir(appDir).absoluteFilePath(                             // 安装后的标准路径
            QStringLiteral("../../share/icons/hicolor/256x256/apps/7zip-gui-cpp.png")),
        QStandardPaths::locate(QStandardPaths::GenericDataLocation,  // 系统全局图标
                               QStringLiteral("icons/hicolor/256x256/apps/7zip-gui-cpp.png")),
    };

    for (const QString &candidate : candidates) {
        if (candidate.isEmpty() || !QFileInfo::exists(candidate))
            continue;
        const QIcon icon(candidate);
        if (!icon.isNull())
            return icon;
    }

    return {};
}

/** 加载 Test 操作图标 */
static QIcon testActionIcon()
{
    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
        QDir(appDir).absoluteFilePath(
            QStringLiteral("../packaging/icons/icons8-test-50.png")),
        QDir(appDir).absoluteFilePath(
            QStringLiteral("../../share/7zip-gui-cpp/icons/icons8-test-50.png")),
        QStandardPaths::locate(
            QStandardPaths::GenericDataLocation,
            QStringLiteral("7zip-gui-cpp/icons/icons8-test-50.png")),
    };

    for (const QString &candidate : candidates) {
        if (candidate.isEmpty() || !QFileInfo::exists(candidate))
            continue;
        const QIcon icon(candidate);
        if (!icon.isNull())
            return icon;
    }

    return themeIcon("system-run");                                // 回退到系统运行图标
}

/** 加载 Extract 操作图标 */
static QIcon extractActionIcon()
{
    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
        QDir(appDir).absoluteFilePath(
            QStringLiteral("../packaging/icons/icons8-extract-50.png")),
        QDir(appDir).absoluteFilePath(
            QStringLiteral("../../share/7zip-gui-cpp/icons/icons8-extract-50.png")),
        QStandardPaths::locate(
            QStandardPaths::GenericDataLocation,
            QStringLiteral("7zip-gui-cpp/icons/icons8-extract-50.png")),
    };

    for (const QString &candidate : candidates) {
        if (candidate.isEmpty() || !QFileInfo::exists(candidate))
            continue;
        const QIcon icon(candidate);
        if (!icon.isNull())
            return icon;
    }

    return themeIcon("archive-extract");                           // 回退到系统解压图标
}

// ─── 测试输出摘要 ────────────────────────────────────────────

/**
 * 从 7z 测试输出中提取关键错误行
 * "Everything is Ok" → 成功摘要，否则提取包含 error/cannot 的行
 */
static QString summarizeTestOutput(const QString &output)
{
    const QString normalized = output;
    const bool ok = normalized.contains(QStringLiteral("Everything is Ok"), Qt::CaseInsensitive);
    if (ok)
        return QObject::tr("Archive OK.");

    QStringList lines;
    const QStringList all = normalized.split(QLatin1Char('\n'));
    for (const QString &raw : all) {
        QString line = raw;
        line.remove(QLatin1Char('\r'));                            // 去掉回车
        line.remove(QLatin1Char('\b'));                            // 去掉退格
        const QString lower = line.toLower();
        if (lower.contains(QStringLiteral("error")) || lower.contains(QStringLiteral("errors"))
            || lower.contains(QStringLiteral("wrong password"))
            || lower.contains(QStringLiteral("cannot"))
            || lower.contains(QStringLiteral("can not"))
            || lower.contains(QStringLiteral("sub items"))
            || lower.contains(QStringLiteral("archives with errors"))) {
            lines << line.trimmed();
        }
    }
    lines.removeAll(QString());
    lines.removeDuplicates();
    if (lines.isEmpty())
        return QObject::tr("Test failed. See details in /tmp/7zip-gui-cpp-last-test.txt");
    return lines.mid(0, 8).join(QLatin1Char('\n'));               // 最多 8 行
}

// ─── 默认压缩包路径 ──────────────────────────────────────────

/**
 * 根据输入文件列表生成默认的压缩包保存路径
 * 单个文件/文件夹 → 同目录下同名 .7z；多个 → archive.7z
 */
static QString defaultArchivePathForInputs(const QStringList &inputs)
{
    const QString home = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    if (inputs.isEmpty())
        return home + QStringLiteral("/archive.7z");

    const QFileInfo fi(inputs.first());
    // 单个选中的文件夹 "A" → 父目录/A.7z，而不是 A/A.7z
    const QString dir = fi.absolutePath();
    QString name;
    if (inputs.size() == 1)
        name = (fi.isDir() ? fi.fileName() : fi.completeBaseName()) + QStringLiteral(".7z");
    else
        name = QStringLiteral("archive.7z");

    QString candidate = dir + QLatin1Char('/') + name;
    if (!QFileInfo::exists(candidate))
        return candidate;

    // 同名文件已存在时附加序号：archive_1.7z, archive_2.7z...
    const int dot = name.lastIndexOf(QLatin1Char('.'));
    const QString stem = dot > 0 ? name.left(dot) : name;
    const QString ext = dot > 0 ? name.mid(dot) : QString();
    for (int n = 1; n < 10000; ++n) {
        candidate = dir + QLatin1Char('/') + stem + QLatin1Char('_') + QString::number(n) + ext;
        if (!QFileInfo::exists(candidate))
            return candidate;
    }
    return dir + QLatin1Char('/') + stem + QStringLiteral("_new") + ext;  // 兜底
}

// ─── MainWindow 构造/析构 ────────────────────────────────────

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    // 注册自定义类型以支持跨线程信号槽
    qRegisterMetaType<QVector<ArchiveEntry>>("QVector<ArchiveEntry>");

    setupUi();

    // 创建工作线程并将 Worker 移入
    m_thread = new QThread(this);
    m_worker = new Worker;
    m_worker->moveToThread(m_thread);
    connect(m_thread, &QThread::finished, m_worker, &QObject::deleteLater);  // 线程结束时清理 Worker

    // 永久信号连接 — Worker → MainWindow
    connect(m_worker, &Worker::listFinished, this, &MainWindow::onListFinished);
    connect(m_worker, &Worker::openStageChanged, this, &MainWindow::onOpenStageChanged);
    connect(m_worker, &Worker::openProbeFinished, this, &MainWindow::onOpenProbeFinished);
    connect(m_worker, &Worker::openPasswordValidated, this, &MainWindow::onOpenPasswordValidated);
    connect(m_worker, &Worker::error, this, &MainWindow::onWorkerError);
    connect(m_worker, &Worker::progress, this, &MainWindow::onProgress);

    // 大型压缩包打开提示定时器（5 秒后触发）
    m_openSlowHintTimer = new QTimer(this);
    m_openSlowHintTimer->setSingleShot(true);
    m_openSlowHintTimer->setInterval(kOpenSlowHintMs);
    connect(m_openSlowHintTimer, &QTimer::timeout, this, &MainWindow::onOpenSlowHintTimeout);

    m_thread->start();
    statusBar()->showMessage(tr("Open an archive to begin"));
}

MainWindow::~MainWindow()
{
    if (m_thread) {
        m_thread->quit();        // 请求线程退出
        m_thread->wait(5000);    // 等待最多 5 秒
    }
}

// ─── 模式设置 ────────────────────────────────────────────────

void MainWindow::setHeadlessAddMode(bool enabled)
{
    m_headlessAddMode = enabled;                                   // CLI --add 模式
}

void MainWindow::deferInitialShowUntilOpenCompletes(bool enabled)
{
    m_deferInitialShow = enabled;                                  // CLI --open 模式先打开再显示
}

// ─── 忙碌状态锁定 ────────────────────────────────────────────

/** 设置忙碌状态，禁用所有工具栏按钮 */
void MainWindow::setBusy(bool busy)
{
    m_busy = busy;
    for (QAction *a : findChildren<QAction *>())
        a->setEnabled(!busy);
}

/** 如果正忙则提示并返回 false */
bool MainWindow::ensureNotBusy()
{
    if (m_busy) {
        statusBar()->showMessage(tr("Please wait for the current operation to finish."));
        return false;
    }
    return true;
}

// ─── UI 搭建 ─────────────────────────────────────────────────

void MainWindow::setupUi()
{
    setWindowTitle(tr("7-Zip File Manager"));
    resize(980, 620);

    // 中央区域：路径标签 + 文件列表表格
    auto *central = new QWidget;
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(8);

    m_pathLabel = new QLabel(tr("No archive open"));
    m_pathLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);  // 允许复制路径
    m_pathLabel->setWordWrap(true);
    layout->addWidget(m_pathLabel);

    m_table = new QTableView;
    m_table->setModel(&m_model);
    m_table->setAlternatingRowColors(true);                          // 交替行颜色
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);    // 整行选中
    m_table->setSelectionMode(QAbstractItemView::ExtendedSelection); // 允许多选
    m_table->horizontalHeader()->setStretchLastSection(true);        // 最后一列拉伸
    m_table->horizontalHeader()->setSectionResizeMode(ArchiveModel::Name, QHeaderView::Stretch);
    m_table->verticalHeader()->setVisible(false);                    // 隐藏行号
    m_table->setShowGrid(false);
    layout->addWidget(m_table, 1);                                   // stretch=1 填充剩余空间

    setCentralWidget(central);

    // 工具栏
    auto *toolbar = addToolBar(tr("Archive"));
    toolbar->setMovable(false);
    toolbar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);       // 图标+文字
    toolbar->setIconSize(QSize(16, 16));

    m_openAct = toolbar->addAction(themeIcon("document-open"), tr("Open"));
    connect(m_openAct, &QAction::triggered, this, &MainWindow::openArchiveDialog);

    toolbar->addSeparator();

    m_extractAct = toolbar->addAction(extractActionIcon(), tr("Extract"));
    connect(m_extractAct, &QAction::triggered, this, &MainWindow::onExtract);

    toolbar->addSeparator();

    m_testAct = toolbar->addAction(testActionIcon(), tr("Test"));
    connect(m_testAct, &QAction::triggered, this, &MainWindow::onTest);

    toolbar->addSeparator();

    m_refreshAct = toolbar->addAction(themeIcon("view-refresh"), tr("Refresh"));
    connect(m_refreshAct, &QAction::triggered, this, &MainWindow::onRefresh);

    toolbar->addSeparator();

    m_aboutAct = toolbar->addAction(themeIcon("help-about"), tr("About"));
    connect(m_aboutAct, &QAction::triggered, this, &MainWindow::onAbout);

    // 弹性空间将语言选择器推到右侧
    auto *spacer = new QWidget(toolbar);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    toolbar->addWidget(spacer);

    // 语言切换下拉框
    m_languageCombo = new QComboBox(toolbar);
    m_languageCombo->setMinimumContentsLength(10);
    m_languageCombo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    toolbar->addWidget(m_languageCombo);
    connect(m_languageCombo, &QComboBox::currentIndexChanged, this,
            &MainWindow::onLanguageSelectionChanged);

    menuBar()->hide();                                               // 隐藏菜单栏
    retranslateUi();
}

// ─── 语言/翻译 ────────────────────────────────────────────────

void MainWindow::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();                                             // 语言变更时刷新所有文字
    QMainWindow::changeEvent(event);
}

/** 刷新所有 UI 文本（支持运行中切换语言） */
void MainWindow::retranslateUi()
{
    setWindowTitle(m_archivePath.isEmpty()
                       ? tr("7-Zip File Manager")
                       : QFileInfo(m_archivePath).fileName() + QStringLiteral(" — ")
                             + tr("7-Zip File Manager"));
    if (m_pathLabel) {
        m_pathLabel->setText(m_archivePath.isEmpty() ? tr("No archive open") : m_archivePath);
    }
    if (m_openAct)
        m_openAct->setText(tr("Open"));
    if (m_extractAct)
        m_extractAct->setText(tr("Extract"));
    if (m_testAct)
        m_testAct->setText(tr("Test"));
    if (m_refreshAct)
        m_refreshAct->setText(tr("Refresh"));
    if (m_aboutAct)
        m_aboutAct->setText(tr("About"));
    if (m_languageCombo) {
        const QSignalBlocker blocker(m_languageCombo);               // 阻止信号触发
        if (m_languageCombo->count() != 2) {
            m_languageCombo->clear();
            m_languageCombo->addItem(QString());                     // English 占位
            m_languageCombo->addItem(QString());                     // 中文 占位
        }
        m_languageCombo->setItemText(0, tr("English"));
        m_languageCombo->setItemText(1, tr("中文"));
        const bool zh = AppLocale::savedLanguage() == QStringLiteral("zh_CN");
        m_languageCombo->setCurrentIndex(zh ? 1 : 0);
    }
}

void MainWindow::setLanguageEnglish()
{
    AppLocale::saveLanguage(QStringLiteral("en"));
    AppLocale::applyLanguage(QStringLiteral("en"), this);
    retranslateUi();
}

void MainWindow::setLanguageChinese()
{
    AppLocale::saveLanguage(QStringLiteral("zh_CN"));
    AppLocale::applyLanguage(QStringLiteral("zh_CN"), this);
    retranslateUi();
}

void MainWindow::onLanguageSelectionChanged(int index)
{
    if (index == 0)
        setLanguageEnglish();
    else if (index == 1)
        setLanguageChinese();
}

// ─── About ────────────────────────────────────────────────────

void MainWindow::onAbout()
{
    QMessageBox box(QMessageBox::NoIcon, tr("About"),
                    tr("7-Zip File Manager\n\nArchive manager for GNOME/Linux using Qt 6 and p7zip."),
                    QMessageBox::Ok, this);
    const QIcon appIcon = aboutDialogIcon();
    if (!appIcon.isNull())
        box.setIconPixmap(appIcon.pixmap(64, 64));                   // 使用大图标
    else
        box.setIcon(QMessageBox::Information);                       // 回退
    DialogUtils::centerPopup(&box, this);
    box.exec();
}

// ─── 操作结果 ────────────────────────────────────────────────

void MainWindow::showOperationResult(OperationResultDialog::Kind kind, const OperationStats &stats,
                                     const QString &path)
{
    OperationResultDialog dlg(kind, stats, path, this);
    DialogUtils::execCentered(dlg, this);
}

// ─── 确保压缩包已打开 ─────────────────────────────────────────

bool MainWindow::ensureArchiveOpen()
{
    if (m_archivePath.isEmpty()) {
        DialogUtils::showCenteredMessageBox(this, QMessageBox::Information, tr("No archive"),
                                            tr("Open an archive first."));
        return false;
    }
    return true;
}

// ─── 密码提示 ────────────────────────────────────────────────

QString MainWindow::promptPassword()
{
    return promptPasswordForArchive(m_archivePath);
}

/** 弹出密码输入对话框，返回密码；取消时返回空 */
QString MainWindow::promptPasswordForArchive(const QString &archivePath, bool *accepted)
{
    PasswordDialog dlg(QFileInfo(archivePath).fileName(), this);
    const bool ok = (DialogUtils::execCentered(dlg, this) == QDialog::Accepted);
    if (accepted)
        *accepted = ok;
    if (!ok)
        return {};
    return dlg.password();
}

// ─── 打开压缩包 ──────────────────────────────────────────────

void MainWindow::openArchiveDialog()
{
    if (!ensureNotBusy())
        return;
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Open archive"), QString(),
        tr("Archives (*.7z *.zip *.tar *.gz *.bz2 *.xz *.rar *.iso);;All files (*)"));
    if (!path.isEmpty())
        openArchive(path);
}

void MainWindow::openArchive(const QString &path)
{
    if (!ensureNotBusy())
        return;
    if (!QFileInfo::exists(path)) {
        DialogUtils::showCenteredMessageBox(this, QMessageBox::Warning, tr("Open archive"),
                                            tr("File does not exist."));
        return;
    }
    m_openProbeEntries.clear();
    resetArchiveView();
    beginOpenProbe(path);                                            // 开始探测阶段
}

// ─── 打开 → 探测阶段 ──────────────────────────────────────────

/** 发起探测：不带密码列出 + 测试，判断是否加密 */
void MainWindow::beginOpenProbe(const QString &path)
{
    m_pendingOpenPath = path;
    m_lastOp = LastOp::OpenProbe;
    m_openProbeInFlight = true;                                      // 标记探测进行中
    m_openProbeCompleted = false;
    m_openProbeEncryptedSignalDetected = false;
    m_openPendingPassword.clear();
    setBusy(true);
    m_openSlowHintShown = false;
    updateOpenStatus(tr("Reading archive header..."));
    startOpenSlowHintTimer();                                        // 启动慢速提示定时器
    QMetaObject::invokeMethod(m_worker, "doProbeForOpen", Qt::QueuedConnection, Q_ARG(QString, path));
}

/** 探测完成后调用：加密则提示密码，非加密直接显示内容 */
void MainWindow::onOpenProbeFinished(bool encryptedSignalDetected, QVector<ArchiveEntry> entries)
{
    setBusy(false);
    hideOpenWait();
    stopOpenSlowHintTimer();
    m_openSlowHintShown = false;
    m_openProbeInFlight = false;
    m_openProbeCompleted = true;                                     // 探测完成
    m_openProbeEncryptedSignalDetected = encryptedSignalDetected;
    m_openProbeEntries = std::move(entries);
    m_lastOp = LastOp::None;
    continueOpenAfterProbeIfReady();
}

void MainWindow::continueOpenAfterProbeIfReady()
{
    if (!m_openProbeCompleted)
        return;

    const QString path = m_pendingOpenPath;
    m_pendingOpenPath.clear();
    if (path.isEmpty()) {
        m_openProbeEntries.clear();
        statusBar()->showMessage(tr("Cancelled"));
        maybeShowDeferredMainWindow();
        return;
    }

    // 非加密压缩包：直接应用并显示内容
    if (!m_openProbeEncryptedSignalDetected) {
        QVector<ArchiveEntry> entries = std::move(m_openProbeEntries);
        m_openProbeEntries.clear();
        applyOpenedArchive(path, {});
        m_openAwaitingPasswordValidation = false;
        onListFinished(std::move(entries));
        return;
    }

    // 加密压缩包：弹出密码输入框
    bool accepted = false;
    const QString pwd = promptPasswordForArchive(path, &accepted);
    if (!accepted || pwd.isEmpty()) {
        m_openProbeEntries.clear();
        m_openPendingPassword.clear();
        statusBar()->showMessage(tr("Cancelled"));
        if (m_deferInitialShow)
            QCoreApplication::exit(0);                                // CLI --open 模式退出
        else
            maybeShowDeferredMainWindow();                            // GUI 模式继续显示
        return;
    }

    m_openPendingPassword = pwd;
    applyOpenedArchive(path, m_openPendingPassword);
    m_openAwaitingPasswordValidation = true;                          // 标记等待密码验证
    runList(m_openPendingPassword);                                   // 验证密码并列出
}

/** 确认打开并设置窗口标题和路径标签 */
void MainWindow::applyOpenedArchive(const QString &path, const QString &password)
{
    m_archivePath = path;
    m_password = password;
    m_pathLabel->setText(path);
    setWindowTitle(QFileInfo(path).fileName() + QStringLiteral(" — ") + tr("7-Zip File Manager"));
}

/** 密码验证通过后直接显示已缓存的条目列表 */
void MainWindow::onOpenPasswordValidated()
{
    if (!m_openAwaitingPasswordValidation)
        return;
    onListFinished(std::move(m_openProbeEntries));
}

// ─── 打开等待对话框 ──────────────────────────────────────────

void MainWindow::showOpenWait(const QString &label)
{
    if (!m_openWaitDialog) {
        m_openWaitDialog = new QProgressDialog(this);
        m_openWaitDialog->setWindowModality(Qt::NonModal);            // 非模态
        m_openWaitDialog->setRange(0, 0);                             // 不确定进度
        m_openWaitDialog->setCancelButton(nullptr);                   // 无取消按钮
        m_openWaitDialog->setMinimumDuration(0);                      // 立即显示
        m_openWaitDialog->setAutoClose(false);
        m_openWaitDialog->setAutoReset(false);
        m_openWaitDialog->setWindowFlag(Qt::WindowStaysOnTopHint, true);
        connect(m_openWaitDialog, &QProgressDialog::canceled, this, &MainWindow::onOpenWaitCanceled);

        m_openWaitDialog->adjustSize();
        const int baseWidth = m_openWaitDialog->sizeHint().width();
        if (baseWidth > 0)
            m_openWaitDialog->setMinimumWidth(baseWidth * 2);
    }
    m_openWaitDialog->setWindowTitle(tr("Opening archive"));
    m_openWaitDialog->setLabelText(label);
    centerOpenWaitOnScreen();
    if (!m_openWaitDialog->isVisible())
        m_openWaitDialog->show();
}

void MainWindow::hideOpenWait()
{
    if (m_openWaitDialog)
        m_openWaitDialog->hide();
}

/** 将等待对话框居中放置在屏幕中央 */
void MainWindow::centerOpenWaitOnScreen()
{
    if (!m_openWaitDialog)
        return;

    m_openWaitDialog->adjustSize();
    QScreen *screen = m_openWaitDialog->screen();
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    if (!screen)
        return;

    const QRect available = screen->availableGeometry();
    const QSize dialogSize = m_openWaitDialog->frameGeometry().size();
    const QPoint topLeft(available.center().x() - dialogSize.width() / 2,
                         available.center().y() - dialogSize.height() / 2);
    m_openWaitDialog->move(topLeft);
}

// ─── 延时显示 ────────────────────────────────────────────────

/**
 * CLI --open 模式下延迟显示主窗口
 * 在探测或密码验证进行中不显示；两个都完成后再显示
 */
void MainWindow::maybeShowDeferredMainWindow()
{
    if (!m_deferInitialShow)
        return;
    if (m_openProbeInFlight || m_openAwaitingPasswordValidation)
        return;

    m_deferInitialShow = false;
    show();
}

void MainWindow::onOpenWaitCanceled()
{
    if (m_openProbeInFlight || m_openAwaitingPasswordValidation)
        QCoreApplication::exit(0);                                   // 用户取消等待 → 退出
}

void MainWindow::updateOpenStatus(const QString &message, bool updateDialog)
{
    statusBar()->showMessage(message);
    if (updateDialog)
        showOpenWait(message);                                       // 同步更新等待对话框
}

// ─── 慢速提示 ────────────────────────────────────────────────

void MainWindow::startOpenSlowHintTimer()
{
    if (!m_openSlowHintTimer)
        return;
    m_openSlowHintTimer->stop();                                     // 重置计时
    m_openSlowHintTimer->start();
}

void MainWindow::stopOpenSlowHintTimer()
{
    if (m_openSlowHintTimer)
        m_openSlowHintTimer->stop();
}

void MainWindow::onOpenStageChanged(const QString &message)
{
    if (message.isEmpty())
        return;
    updateOpenStatus(message);
}

void MainWindow::onOpenSlowHintTimeout()
{
    if (m_openSlowHintShown || (!m_openProbeInFlight && !m_openAwaitingPasswordValidation))
        return;
    m_openSlowHintShown = true;
    updateOpenStatus(tr("Archive is large, still processing..."));
}

// ─── 列出压缩包内容 ──────────────────────────────────────────

/** 发起列出操作（加密压缩包先验证密码再列出，非加密直接列出） */
void MainWindow::runList(const QString &password)
{
    if (!password.isEmpty())
        m_password = password;
    m_lastOp = LastOp::List;
    if (m_openAwaitingPasswordValidation)
        updateOpenStatus(tr("Validating password..."));
    else
        statusBar()->showMessage(tr("Loading file list..."));
    if (m_openAwaitingPasswordValidation)
        startOpenSlowHintTimer();
    setBusy(true);
    if (m_openAwaitingPasswordValidation) {
        QMetaObject::invokeMethod(m_worker, "doListValidated", Qt::QueuedConnection,  // 带验证列出
                                  Q_ARG(QString, m_archivePath), Q_ARG(QString, m_password));
        return;
    }
    QMetaObject::invokeMethod(m_worker, "doList", Qt::QueuedConnection,      // 直接列出
                              Q_ARG(QString, m_archivePath),
                              Q_ARG(QString, m_password));
}

/** 列出结果回调：填充表格模型 */
void MainWindow::onListFinished(QVector<ArchiveEntry> entries)
{
    setBusy(false);
    hideOpenWait();
    stopOpenSlowHintTimer();
    m_openSlowHintShown = false;
    if (m_openAwaitingPasswordValidation)
        m_openAwaitingPasswordValidation = false;
    if (m_password.isEmpty() && hasEncryptedEntrySignal(entries)) {
        // 密码未验证前不渲染加密条目（防止意外暴露加密元数据）
        m_model.setEntries({});
        statusBar()->showMessage(tr("Password required"));
        maybeShowDeferredMainWindow();
        return;
    }
    m_model.setEntries(std::move(entries));
    statusBar()->showMessage(tr("%1 entries").arg(m_model.rowCount()));
    maybeShowDeferredMainWindow();
}

// ─── 重置视图 ────────────────────────────────────────────────

void MainWindow::resetArchiveView()
{
    m_archivePath.clear();
    m_password.clear();
    m_openAwaitingPasswordValidation = false;
    m_openProbeInFlight = false;
    m_openSlowHintShown = false;
    m_openProbeCompleted = false;
    m_openProbeEncryptedSignalDetected = false;
    m_openProbeEntries.clear();
    m_openPendingPassword.clear();
    m_pendingOpenPath.clear();
    m_lastOp = LastOp::None;
    stopOpenSlowHintTimer();
    m_model.setEntries({});
    hideOpenWait();
    m_pathLabel->setText(tr("No archive open"));
    setWindowTitle(tr("7-Zip File Manager"));
}

bool MainWindow::hasEncryptedEntrySignal(const QVector<ArchiveEntry> &entries)
{
    for (const ArchiveEntry &entry : entries) {
        if (entry.encrypted == QLatin1Char('+'))
            return true;
    }
    return false;
}

// ─── 错误处理 ────────────────────────────────────────────────

void MainWindow::onWorkerError(QString message, bool passwordError)
{
    // 被进度对话框包裹的操作 → 错误由进度框 lambda 处理，不重复弹窗
    if (m_progressActive)
        return;
    if (m_errorAlreadyHandled) {
        m_errorAlreadyHandled = false;
        return;
    }

    // 探测阶段错误 → 直接报错并清理
    if (m_openProbeInFlight) {
        m_openProbeInFlight = false;
        m_openSlowHintShown = false;
        stopOpenSlowHintTimer();
        m_openProbeEntries.clear();
        m_pendingOpenPath.clear();
        m_lastOp = LastOp::None;
        setBusy(false);
        hideOpenWait();
        DialogUtils::showCenteredMessageBox(this, QMessageBox::Critical, tr("Error"), message);
        QCoreApplication::exit(1);
        return;
    }

    setBusy(false);
    hideOpenWait();
    stopOpenSlowHintTimer();

    if (passwordError) {
        if (m_openAwaitingPasswordValidation) {
            DialogUtils::showCenteredMessageBox(this, QMessageBox::Warning, tr("Wrong password"),
                                                tr("Password is incorrect. Please try again."));
        }

        QString pwd;
        if (m_lastOp == LastOp::Add && m_archivePath.isEmpty()) {
            // 创建压缩包时密码错误的特殊路径（使用 pending 中的信息）
            PasswordDialog dlg(m_pendingAddArchive.isEmpty()
                                   ? tr("new archive")
                                   : QFileInfo(m_pendingAddArchive).fileName(),
                               this);
            if (DialogUtils::execCentered(dlg, this) != QDialog::Accepted) {
                statusBar()->showMessage(tr("Cancelled"));
                return;
            }
            pwd = dlg.password();
            m_pendingAddPassword = pwd;
        } else {
            pwd = promptPassword();
            if (pwd.isEmpty()) {
                if (m_openAwaitingPasswordValidation) {
                    resetArchiveView();
                    QCoreApplication::exit(0);                       // Open 流程取消 → 退出
                    return;
                }
                m_openAwaitingPasswordValidation = false;
                statusBar()->showMessage(tr("Cancelled"));
                maybeShowDeferredMainWindow();
                return;
            }
            m_password = pwd;
        }
        retryLastOperation();                                        // 使用新密码重试
        return;
    }

    // 非密码错误 → 显示错误并重置
    if (m_openAwaitingPasswordValidation)
        resetArchiveView();
    else
        m_openAwaitingPasswordValidation = false;
    DialogUtils::showCenteredMessageBox(this, QMessageBox::Critical, tr("Error"), message);
    statusBar()->showMessage(tr("Error"));
    maybeShowDeferredMainWindow();
}

// ─── 重试上一操作 ────────────────────────────────────────────

void MainWindow::retryLastOperation()
{
    switch (m_lastOp) {
    case LastOp::List:
        runList(m_password);
        break;
    case LastOp::Extract:
        if (!m_pendingExtractDir.isEmpty())
            runExtractToDir(m_pendingExtractDir);                    // 保留原有目标路径
        break;
    case LastOp::Test:
        runTestArchive();
        break;
    case LastOp::Add:
        if (!m_pendingAddArchive.isEmpty() && !m_pendingAddFiles.isEmpty())
            runAddArchive(m_pendingAddArchive, m_pendingAddFiles, m_pendingAddFormat,
                          m_pendingAddLevel, m_pendingAddPassword);   // 保留所有参数
        break;
    case LastOp::None:
        if (!m_archivePath.isEmpty())
            runList(m_password);
        break;
    }
}

// ─── 刷新 ────────────────────────────────────────────────────

void MainWindow::onRefresh()
{
    if (!ensureNotBusy() || !ensureArchiveOpen())
        return;
    runList(m_password);
}

// ─── 解压 ────────────────────────────────────────────────────

void MainWindow::onExtract()
{
    if (!ensureNotBusy() || !ensureArchiveOpen())
        return;

    const QFileInfo fi(m_archivePath);
    // 默认解压目标：压缩包同目录下以压缩包名命名的文件夹
    QString defaultDir = fi.absolutePath() + QLatin1Char('/') + fi.completeBaseName();
    if (!m_model.entries().isEmpty()) {
        const ExtractDestination hint = ArchiveUtils::resolveExtractDestinationFromEntries(
            fi.absolutePath(), m_archivePath, m_model.entries());
        defaultDir = hint.outputDir;
        if (defaultDir.endsWith(QLatin1Char('/')))
            defaultDir.chop(1);                                      // 去掉尾部斜杠
    }

    ExtractDialog dlg(defaultDir, this);
    if (DialogUtils::execCentered(dlg, this) != QDialog::Accepted)
        return;

    runExtractToDir(dlg.outputDirectory());
}

bool MainWindow::runExtractToDir(const QString &userBaseDir)
{
    m_lastOp = LastOp::Extract;
    m_pendingExtractDir = userBaseDir;
    m_operationTimer.start();

    // 确定解压目标路径
    ExtractDestination dest;
    if (!m_model.entries().isEmpty()) {
        dest = ArchiveUtils::resolveExtractDestinationFromEntries(userBaseDir, m_archivePath,
                                                                  m_model.entries());
    } else {
        dest = ArchiveUtils::fallbackExtractDestination(userBaseDir, m_archivePath);
    }

    // 预计算统计数据
    m_lastExtractOutputPath = dest.outputDir;
    m_lastExtractStats.sizeBefore = QFileInfo(m_archivePath).size();
    m_lastExtractStats.sizeAfter =
        m_model.entries().isEmpty() ? 0 : ArchiveUtils::entriesUncompressedSize(m_model.entries());
    m_lastExtractStats.isCompress = false;

    while (true) {
        ProgressDialog progress(tr("Extracting"), this);
        progress.setLabel(tr("Preparing extraction…"));
        progress.setIndeterminate(true);                              // 开始时不确���进度
        statusBar()->showMessage(tr("Preparing extraction…"));

        bool passwordError = false;
        QString errorMessage;
        bool numericProgressSeen = false;

        QMetaObject::Connection c1 = connect(
            m_worker, &Worker::progress, &progress,
            [&progress, this, &numericProgressSeen](int percent) {
                if (!numericProgressSeen) {
                    numericProgressSeen = true;
                    progress.setIndeterminate(false);
                    progress.setLabel(tr("Extracting archive…"));
                    statusBar()->showMessage(tr("Extracting archive…"));
                }
                progress.setProgress(percent);
            });
        QMetaObject::Connection c2 = connect(m_worker, &Worker::extractFinished, &progress,
                                             [&progress]() { progress.accept(); });
        QMetaObject::Connection c3 = connect(
            m_worker, &Worker::error, &progress, [&](const QString &msg, bool pwdErr) {
                passwordError = pwdErr;
                errorMessage = msg;
                progress.reject();
            });

        m_progressActive = true;
        m_errorAlreadyHandled = false;                                // 重置本迭代的错误标记
        setBusy(true);
        QMetaObject::invokeMethod(m_worker, "doExtract", Qt::QueuedConnection,
                                  Q_ARG(QString, m_archivePath), Q_ARG(QString, dest.outputDir),
                                  Q_ARG(QString, m_password));

        const int rc = DialogUtils::execCentered(progress, this);
        disconnect(c1);
        disconnect(c2);
        disconnect(c3);
        m_progressActive = false;
        setBusy(false);

        if (rc == QDialog::Accepted) {
            onExtractFinished();
            return true;
        }

        if (passwordError) {
            m_errorAlreadyHandled = true;
            const QString pwd = promptPassword();
            if (pwd.isEmpty()) {
                statusBar()->showMessage(tr("Cancelled"));
                return false;
            }
            m_password = pwd;
            continue;                                                 // 新密码重试
        }

        m_errorAlreadyHandled = true;
        if (!errorMessage.isEmpty())
            DialogUtils::showCenteredMessageBox(this, QMessageBox::Critical, tr("Error"),
                                                errorMessage);
        return false;
    }
}

void MainWindow::onExtractFinished()
{
    m_lastExtractStats.elapsedMs = m_operationTimer.elapsed();
    showOperationResult(OperationResultDialog::Kind::Extract, m_lastExtractStats,
                        m_lastExtractOutputPath);
    statusBar()->showMessage(tr("Extraction complete"));
    QCoreApplication::exit(0);                                       // 解压完成后退出
}

// ─── 测试 ────────────────────────────────────────────────────

void MainWindow::onTest()
{
    if (!ensureNotBusy() || !ensureArchiveOpen())
        return;
    runTestArchive();
}

bool MainWindow::runTestArchive()
{
    m_lastOp = LastOp::Test;
    SevenZipBackend backend;
    // 启动阶段进度框
    ProgressDialog startupProgress(tr("Testing archive"), this);
    startupProgress.setIndeterminate(true);
    startupProgress.setLabel(tr("Preparing test..."));
    DialogUtils::centerPopup(&startupProgress, this);
    startupProgress.show();
    QCoreApplication::processEvents();                               // 立即渲染
    bool archiveEncrypted = false;
    try {
        archiveEncrypted = backend.hasEncryptedEntries(m_archivePath);
    } catch (const SevenZipError &e) {
        startupProgress.hide();
        DialogUtils::showCenteredMessageBox(this, QMessageBox::Critical, tr("Error"), e.message());
        QCoreApplication::exit(1);
        return false;
    }
    startupProgress.hide();

    while (true) {
        QString password;
        if (archiveEncrypted) {
            bool accepted = false;
            password = promptPasswordForArchive(m_archivePath, &accepted);
            if (!accepted || password.isEmpty()) {
                QCoreApplication::exit(0);
                return false;
            }
        }

        ProgressDialog progress(tr("Testing archive"), this);
        progress.setIndeterminate(true);
        progress.setLabel(archiveEncrypted ? tr("Validating password...") : tr("Testing archive integrity..."));
        statusBar()->showMessage(archiveEncrypted ? tr("Validating password...")
                                                  : tr("Testing archive integrity..."));

        QString testOutput;
        QString errorMessage;
        bool passwordError = false;
        bool switchedToIntegrityStage = false;

        QMetaObject::Connection c1 = connect(
            m_worker, &Worker::progress, &progress,
            [&progress, this, &switchedToIntegrityStage](int percent) {
                if (!switchedToIntegrityStage) {
                    switchedToIntegrityStage = true;
                    progress.setLabel(tr("Testing archive integrity..."));
                    progress.setIndeterminate(false);
                    statusBar()->showMessage(tr("Testing archive integrity..."));
                }
                progress.setProgress(percent);
            });
        QMetaObject::Connection c2 = connect(
            m_worker, &Worker::testFinished, &progress, [&](const QString &out) {
                testOutput = out;
                progress.setLabel(tr("Preparing result..."));
                progress.setIndeterminate(true);
                statusBar()->showMessage(tr("Preparing result..."));
                QCoreApplication::processEvents();
                progress.accept();
            });
        QMetaObject::Connection c3 = connect(
            m_worker, &Worker::error, &progress, [&](const QString &msg, bool pwdErr) {
                errorMessage = msg;
                passwordError = pwdErr;
                progress.reject();
            });

        m_progressActive = true;
        m_errorAlreadyHandled = false;
        setBusy(true);
        QMetaObject::invokeMethod(m_worker, "doTest", Qt::QueuedConnection, Q_ARG(QString, m_archivePath),
                                  Q_ARG(QString, password));
        const int rc = DialogUtils::execCentered(progress, this);
        disconnect(c1);
        disconnect(c2);
        disconnect(c3);
        m_progressActive = false;
        setBusy(false);

        if (rc == QDialog::Accepted) {
            onTestFinished(testOutput);
            return true;
        }
        if (passwordError) {
            m_errorAlreadyHandled = true;
            DialogUtils::showCenteredMessageBox(this, QMessageBox::Warning, tr("Wrong password"),
                                                tr("Password is incorrect. Please try again."));
            continue;
        }
        m_errorAlreadyHandled = true;
        DialogUtils::showCenteredMessageBox(this, QMessageBox::Critical, tr("Test failed"),
                                            errorMessage);
        return false;
    }
}

void MainWindow::onTestFinished(QString output)
{
    QFile file(QStringLiteral("/tmp/7zip-gui-cpp-last-test.txt"));
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        file.write(output.toUtf8());                                 // 保存完整测试输出
    DialogUtils::showCenteredMessageBox(this, QMessageBox::Information, tr("Test"),
                                        summarizeTestOutput(output));
    statusBar()->showMessage(tr("Test complete"));
}

// ─── 创建压缩包 ──────────────────────────────────────────────

void MainWindow::startAddToArchive(const QStringList &inputPaths)
{
    if (!ensureNotBusy())
        return;
    if (inputPaths.isEmpty())
        onAddToArchive();                                            // 弹出文件选择对话框
    else
        onAddToArchiveWithInputs(inputPaths);                        // CLI 直接使用传入的文件
}

void MainWindow::onAddToArchive()
{
    if (!ensureNotBusy())
        return;
    if (!m_archivePath.isEmpty()) {
        DialogUtils::showCenteredMessageBox(
            this, QMessageBox::Information, tr("Add to archive"),
            tr("Creating a new archive while browsing an open archive is not supported.\n"
               "Close the archive or use File → Open after creating it."));
        return;
    }

    // 先尝试选择文件，如果用户取消则尝试选择文件夹
    QStringList paths = QFileDialog::getOpenFileNames(
        this, tr("Files to compress"), QDir::homePath(), tr("All files (*)"));
    if (paths.isEmpty()) {
        const QString dir =
            QFileDialog::getExistingDirectory(this, tr("Folder to compress"), QDir::homePath());
        if (dir.isEmpty())
            return;
        paths << dir;
    }
    onAddToArchiveWithInputs(paths);
}

void MainWindow::onAddToArchiveWithInputs(const QStringList &inputPaths)
{
    if (inputPaths.isEmpty())
        return;

    AddDialog dlg(defaultArchivePathForInputs(inputPaths), this);
    const auto opts = dlg.options();
    if (!opts)
        return;                                                      // 用户取消

    // 检查是否覆盖已有文件
    if (QFileInfo::exists(opts->archivePath)) {
        const auto ans = DialogUtils::showCenteredMessageBox(
            this, QMessageBox::Question, tr("Overwrite?"),
            tr("\"%1\" already exists. Replace it?").arg(opts->archivePath),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (ans != QMessageBox::Yes)
            return;
    }

    m_lastCreatedArchivePath = opts->archivePath;
    runAddArchive(opts->archivePath, inputPaths, opts->format, opts->level, opts->password);
}

bool MainWindow::runAddArchive(const QString &archivePath, const QStringList &files,
                               const QString &format, int level, const QString &password)
{
    m_lastOp = LastOp::Add;
    m_pendingAddArchive = archivePath;
    m_pendingAddFiles = files;
    m_pendingAddFormat = format;
    m_pendingAddLevel = level;
    m_pendingAddPassword = password;
    m_pendingCompressSizeBefore = -1;
    // 异步计算文件总大小（可能很慢，后台算）
    m_pendingCompressSizeFuture =
        QtConcurrent::run([files]() { return ArchiveUtils::pathsTotalSize(files); });
    m_operationTimer.start();

    while (true) {
        ProgressDialog progress(tr("Creating archive"), this);
        progress.setLabel(tr("Compressing…"));

        bool passwordError = false;
        QString errorMessage;
        bool finalizingShown = false;                                // 是否已显示"正在收尾"

        QMetaObject::Connection c1 = connect(
            m_worker, &Worker::progress, &progress, [&progress, this, &finalizingShown](int percent) {
                if (percent >= 100) {
                    if (!finalizingShown) {
                        finalizingShown = true;
                        progress.setLabel(tr("Finalizing archive…"));
                        progress.setIndeterminate(true);
                        statusBar()->showMessage(tr("Finalizing archive…"));
                    }
                    return;
                }
                progress.setProgress(percent);
            });
        QMetaObject::Connection c2 = connect(m_worker, &Worker::addFinished, &progress, [&progress]() {
            progress.accept();
        });
        QMetaObject::Connection c3 = connect(
            m_worker, &Worker::error, &progress, [&](const QString &msg, bool pwdErr) {
                passwordError = pwdErr;
                errorMessage = msg;
                progress.reject();
            });

        m_progressActive = true;
        m_errorAlreadyHandled = false;
        setBusy(true);
        QMetaObject::invokeMethod(
            m_worker, "doAdd", Qt::QueuedConnection, Q_ARG(QString, archivePath),
            Q_ARG(QStringList, files), Q_ARG(QString, m_pendingAddPassword), Q_ARG(QString, format),
            Q_ARG(int, level));

        const int rc = DialogUtils::execCentered(progress, this);
        disconnect(c1);
        disconnect(c2);
        disconnect(c3);
        m_progressActive = false;
        setBusy(false);

        if (rc == QDialog::Accepted) {
            onAddFinished();
            return true;
        }

        if (passwordError) {
            m_errorAlreadyHandled = true;
            PasswordDialog dlg(m_pendingAddArchive.isEmpty() ? tr("new archive")
                                                             : QFileInfo(m_pendingAddArchive).fileName(),
                               this);
            if (DialogUtils::execCentered(dlg, this) != QDialog::Accepted) {
                statusBar()->showMessage(tr("Cancelled"));
                return false;
            }
            m_pendingAddPassword = dlg.password();
            continue;
        }

        m_errorAlreadyHandled = true;
        if (!errorMessage.isEmpty())
            DialogUtils::showCenteredMessageBox(this, QMessageBox::Critical, tr("Error"),
                                                errorMessage);
        statusBar()->showMessage(tr("Archive creation cancelled or failed"));
        return false;
    }
}

void MainWindow::onAddFinished()
{
    const QString path = m_lastCreatedArchivePath;
    m_lastCompressStats.elapsedMs = m_operationTimer.elapsed();
    // 等待异步大小计算完成并读取结果
    if (m_pendingCompressSizeFuture.isValid() && m_pendingCompressSizeFuture.isFinished())
        m_pendingCompressSizeBefore = m_pendingCompressSizeFuture.result();
    m_lastCompressStats.sizeBefore = m_pendingCompressSizeBefore;
    m_lastCompressStats.sizeAfter = QFileInfo::exists(path) ? QFileInfo(path).size() : 0;
    m_lastCompressStats.isCompress = true;
    showOperationResult(OperationResultDialog::Kind::Compress, m_lastCompressStats, path);

    if (m_headlessAddMode) {
        statusBar()->showMessage(tr("Archive created"));
        return;
    }

    // GUI 模式：询问是否打开新创建的压缩包
    const auto ans = DialogUtils::showCenteredMessageBox(
        this, QMessageBox::Question, tr("Archive created"),
        tr("Archive saved to:\n%1\n\nOpen it now?").arg(path), QMessageBox::Yes | QMessageBox::No,
        QMessageBox::Yes);
    statusBar()->showMessage(tr("Archive created"));
    if (ans == QMessageBox::Yes && QFileInfo::exists(path))
        openArchive(path);
}

// ─── 进度 ────────────────────────────────────────────────────

/** Worker 的进度回调 → 状态栏显示百分比 */
void MainWindow::onProgress(int percent)
{
    statusBar()->showMessage(tr("Progress: %1%").arg(percent));
}
