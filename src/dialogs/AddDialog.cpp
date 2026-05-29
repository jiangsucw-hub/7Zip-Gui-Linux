/** @file AddDialog.cpp — 创建压缩包的参数设置对话框 */

#include "AddDialog.h"
#include "utils/DialogUtils.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QHash>
#include <QPushButton>

/** 压缩格式与文件扩展名的映射 */
static const QHash<QString, QString> kFormatExtensions = {
    {QStringLiteral("7z"), QStringLiteral(".7z")},
    {QStringLiteral("zip"), QStringLiteral(".zip")},
    {QStringLiteral("tar"), QStringLiteral(".tar")},
    {QStringLiteral("gzip"), QStringLiteral(".gz")},
    {QStringLiteral("bzip2"), QStringLiteral(".bz2")},
    {QStringLiteral("xz"), QStringLiteral(".xz")},
    {QStringLiteral("wim"), QStringLiteral(".wim")},
};

AddDialog::AddDialog(const QString &defaultArchivePath, QWidget *parent) : QDialog(parent)
{
    setWindowTitle(tr("Add to archive"));
    setMinimumWidth(672);                                              // ~70% 宽度，容纳所有控件

    auto *grid = new QGridLayout(this);
    grid->setHorizontalSpacing(12);
    grid->setVerticalSpacing(8);

    // ── 行 0：归档路径 ──
    grid->addWidget(new QLabel(tr("Archive:")), 0, 0);
    auto *pathRow = new QHBoxLayout;
    m_pathEdit = new QLineEdit(defaultArchivePath);                    // 预填默认路径
    pathRow->addWidget(m_pathEdit, 1);
    auto *browseBtn = new QPushButton(tr("Browse…"));
    connect(browseBtn, &QPushButton::clicked, this, &AddDialog::browseSavePath);
    pathRow->addWidget(browseBtn);
    grid->addLayout(pathRow, 0, 1);

    // ── 行 1：压缩格式 ──
    grid->addWidget(new QLabel(tr("Format:")), 1, 0);
    m_formatCombo = new QComboBox;
    m_formatCombo->addItems({QStringLiteral("7z"), QStringLiteral("zip"), QStringLiteral("tar"),
                             QStringLiteral("gzip"), QStringLiteral("bzip2"), QStringLiteral("xz"),
                             QStringLiteral("wim")});
    connect(m_formatCombo, &QComboBox::currentTextChanged, this, &AddDialog::onFormatChanged);
    grid->addWidget(m_formatCombo, 1, 1);

    // ── 行 2：压缩级别 ──
    grid->addWidget(new QLabel(tr("Compression:")), 2, 0);
    m_levelCombo = new QComboBox;
    m_levelCombo->addItem(tr("Store"), 0);                             // 仅存储不压缩
    m_levelCombo->addItem(tr("Fastest"), 1);
    m_levelCombo->addItem(tr("Fast"), 3);
    m_levelCombo->addItem(tr("Normal"), 5);                            // 默认
    m_levelCombo->addItem(tr("Maximum"), 7);
    m_levelCombo->addItem(tr("Ultra"), 9);
    m_levelCombo->setCurrentIndex(3);                                  // 默认 Normal
    grid->addWidget(m_levelCombo, 2, 1);

    // ── 行 3：密码 ──
    grid->addWidget(new QLabel(tr("Password:")), 3, 0);
    auto *pwRow = new QHBoxLayout;
    m_passwordEdit = new QLineEdit;
    m_passwordEdit->setEchoMode(QLineEdit::Password);                  // 默认隐藏密码
    pwRow->addWidget(m_passwordEdit, 1);
    auto *showPw = new QCheckBox(tr("Show"));
    connect(showPw, &QCheckBox::toggled, m_passwordEdit, [this](bool on) {
        m_passwordEdit->setEchoMode(on ? QLineEdit::Normal : QLineEdit::Password);
    });
    pwRow->addWidget(showPw);
    grid->addLayout(pwRow, 3, 1);

    // ── 行 4：确认密码 ──
    grid->addWidget(new QLabel(tr("Confirm:")), 4, 0);
    auto *cpwRow = new QHBoxLayout;
    m_confirmEdit = new QLineEdit;
    m_confirmEdit->setEchoMode(QLineEdit::Password);
    cpwRow->addWidget(m_confirmEdit, 1);
    auto *showCpw = new QCheckBox(tr("Show"));
    connect(showCpw, &QCheckBox::toggled, m_confirmEdit, [this](bool on) {
        m_confirmEdit->setEchoMode(on ? QLineEdit::Normal : QLineEdit::Password);
    });
    cpwRow->addWidget(showCpw);
    grid->addLayout(cpwRow, 4, 1);

    // ── 行 5：按钮 ──
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, this, &AddDialog::validateAndAccept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    grid->addWidget(buttons, 5, 0, 1, 2);
}

/** 切换格式时自动更新文件扩展名 */
void AddDialog::onFormatChanged(const QString &format)
{
    updateExtensionForFormat(format);
}

/** 根据选择的格式替换路径中的扩展名 */
void AddDialog::updateExtensionForFormat(const QString &format)
{
    QString path = m_pathEdit->text();
    // 去掉现有扩展名
    for (auto it = kFormatExtensions.cbegin(); it != kFormatExtensions.cend(); ++it) {
        if (path.endsWith(it.value(), Qt::CaseInsensitive)) {
            path.chop(it.value().size());
            break;
        }
    }
    // 追加新扩展名
    path += kFormatExtensions.value(format, QStringLiteral(".") + format);
    m_pathEdit->setText(path);
}

/** 弹出文件保存对话框 */
void AddDialog::browseSavePath()
{
    const QString filter = tr("Archives (*.7z *.zip *.tar *.gz *.bz2 *.xz);;All files (*)");
    const QString path =
        QFileDialog::getSaveFileName(this, tr("Save archive as"), m_pathEdit->text(), filter);
    if (!path.isEmpty())
        m_pathEdit->setText(path);
}

/** 在确定前验证：密码一致 + 路径不为空 */
void AddDialog::validateAndAccept()
{
    if (m_passwordEdit->text() != m_confirmEdit->text()) {
        DialogUtils::showCenteredMessageBox(this, QMessageBox::Warning, tr("Password mismatch"),
                                            tr("Passwords do not match."));
        return;
    }
    if (m_pathEdit->text().trimmed().isEmpty()) {
        DialogUtils::showCenteredMessageBox(this, QMessageBox::Warning, tr("Missing path"),
                                            tr("Choose where to save the archive."));
        return;
    }
    accept();
}

/** 模态运行对话框，返回填充的选项或 nullopt */
std::optional<AddArchiveOptions> AddDialog::options()
{
    if (DialogUtils::execCentered(*this, parentWidget()) != QDialog::Accepted)
        return std::nullopt;                                        // 用户取消

    AddArchiveOptions opts;
    opts.archivePath = m_pathEdit->text().trimmed();
    opts.format = m_formatCombo->currentText();
    opts.level = m_levelCombo->currentData().toInt();
    const QString pw = m_passwordEdit->text();
    opts.password = pw.isEmpty() ? QString() : pw;
    return opts;
}
