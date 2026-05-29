/** @file MainWindow.h — 主窗口类声明 */

#pragma once

#include <QFuture>
#include <QElapsedTimer>
#include <QMainWindow>
#include <QTimer>
#include <memory>

#include "ArchiveModel.h"
#include "Worker.h"
#include "dialogs/OperationResultDialog.h"
#include "utils/ArchiveUtils.h"

class QLabel;
class QProgressDialog;
class QTableView;
class QThread;
class QAction;
class QComboBox;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    void openArchive(const QString &path);                      // 打开指定压缩包
    void startAddToArchive(const QStringList &inputPaths);      // 开始创建压缩包流程
    void setHeadlessAddMode(bool enabled);                      // CLI --add 无头模式
    void deferInitialShowUntilOpenCompletes(bool enabled);      // CLI --open 延迟显示窗口

public slots:
    void openArchiveDialog();                                   // 弹出文件选择对话框并打开

protected:
    void changeEvent(QEvent *event) override;                   // 语言变更时 retranslate

private slots:
    // 打开流程
    void onOpenWaitCanceled();                                  // 用户取消等待 → 退出
    void onOpenStageChanged(const QString &message);            // 打开阶段提示文字更新
    void onOpenSlowHintTimeout();                               // 大型压缩包慢速提示
    void onOpenProbeFinished(bool encryptedSignalDetected, QVector<ArchiveEntry> entries);
    void onOpenPasswordValidated();                             // 密码验证通过

    // 操作入口
    void onAddToArchive();
    void onAddToArchiveWithInputs(const QStringList &inputPaths);
    void onExtract();
    void onTest();
    void onRefresh();

    // Worker 回调
    void onListFinished(QVector<ArchiveEntry> entries);         // 列出完成
    void onWorkerError(QString message, bool passwordError);    // 工作线程错误
    void onExtractFinished();
    void onTestFinished(QString output);
    void onAddFinished();
    void onProgress(int percent);                               // 进度更新（→状态栏）

    // 语言
    void onLanguageSelectionChanged(int index);
    void onAbout();
    void setLanguageEnglish();
    void setLanguageChinese();

private:
    enum class LastOp { None, OpenProbe, List, Extract, Test, Add };  // 上次操作类型，用于重试

    // UI
    void setupUi();
    void retranslateUi();
    void setBusy(bool busy);                                   // 锁定/解锁工具栏
    bool ensureArchiveOpen();                                  // 确保有打开的压缩包
    bool ensureNotBusy();                                      // 确保不忙

    // 打开流程内部函数
    void beginOpenProbe(const QString &path);                   // 发起探测
    void applyOpenedArchive(const QString &path, const QString &password);
    void showOpenWait(const QString &label);
    void hideOpenWait();
    void centerOpenWaitOnScreen();
    void maybeShowDeferredMainWindow();
    void updateOpenStatus(const QString &message, bool updateDialog = true);
    void startOpenSlowHintTimer();
    void stopOpenSlowHintTimer();
    QString promptPassword();
    QString promptPasswordForArchive(const QString &archivePath, bool *accepted = nullptr);
    void continueOpenAfterProbeIfReady();
    void resetArchiveView();                                   // 重置所有打开状态
    static bool hasEncryptedEntrySignal(const QVector<ArchiveEntry> &entries);

    // 核心操作函数（含 while(true) 重试循环）
    void runList(const QString &password = {});
    bool runExtractToDir(const QString &userBaseDir);
    bool runTestArchive();
    bool runAddArchive(const QString &archivePath, const QStringList &files, const QString &format,
                       int level, const QString &password);
    void retryLastOperation();                                 // 密码错误后使用新密码重试
    void showOperationResult(OperationResultDialog::Kind kind, const OperationStats &stats,
                             const QString &path);

    // ── 数据成员 ──

    ArchiveModel m_model;                     // 表格模型

    // 当前打开的压缩包
    QString m_archivePath;
    QString m_password;
    QString m_lastCreatedArchivePath;         // 上次创建的压缩包路径

    // 操作重试状态
    LastOp m_lastOp = LastOp::None;
    QString m_pendingExtractDir;              // 上次解压的目标目录
    QString m_lastExtractOutputPath;
    OperationStats m_lastExtractStats;
    OperationStats m_lastCompressStats;
    QStringList m_pendingAddFiles;
    QString m_pendingAddArchive;
    QString m_pendingAddFormat;
    QString m_pendingAddPassword;
    int m_pendingAddLevel = 5;

    // 状态标记
    bool m_busy = false;                      // 正在执行操作
    bool m_progressActive = false;            // 进度对话框活跃中（抑制 onWorkerError）
    bool m_errorAlreadyHandled = false;       // 错误已被本地处理（抑制重复弹窗）
    bool m_headlessAddMode = false;           // CLI --add 模式
    bool m_openAwaitingPasswordValidation = false;  // 等待密码验证
    bool m_openProbeInFlight = false;         // 探测进行中
    bool m_openSlowHintShown = false;         // 慢速提示已显示
    bool m_openProbeCompleted = false;        // 探测已完成
    bool m_openProbeEncryptedSignalDetected = false;
    bool m_deferInitialShow = false;          // 延迟显示主窗口（--open 模式）

    QVector<ArchiveEntry> m_openProbeEntries; // 探测阶段缓存的条目
    QString m_openPendingPassword;            // 打开的待验证密码
    QElapsedTimer m_operationTimer;           // 操作耗时计时器
    qint64 m_pendingCompressSizeBefore = -1;  // 压缩前总大小（异步计算）
    QFuture<qint64> m_pendingCompressSizeFuture;  // 异步计算 future
    QString m_pendingOpenPath;                // 待打开的路径

    // Widget 指针
    QLabel *m_pathLabel = nullptr;
    QTableView *m_table = nullptr;
    QProgressDialog *m_openWaitDialog = nullptr;
    QTimer *m_openSlowHintTimer = nullptr;
    QThread *m_thread = nullptr;
    Worker *m_worker = nullptr;

    // 工具栏 Action
    QAction *m_openAct = nullptr;
    QAction *m_extractAct = nullptr;
    QAction *m_testAct = nullptr;
    QAction *m_refreshAct = nullptr;
    QAction *m_aboutAct = nullptr;
    QComboBox *m_languageCombo = nullptr;
};
