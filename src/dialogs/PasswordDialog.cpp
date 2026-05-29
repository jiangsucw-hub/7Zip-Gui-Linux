/** @file PasswordDialog.cpp — 密码输入对话框 */

#include "PasswordDialog.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>

PasswordDialog::PasswordDialog(const QString &archiveName, QWidget *parent) : QDialog(parent)
{
    constexpr int kDialogWidthScaleNumerator = 13;
    constexpr int kDialogWidthScaleDenominator = 10;

    setWindowTitle(tr("Password"));
    auto *layout = new QVBoxLayout(this);

    // 提示标签：显示需要密码的压缩包名
    layout->addWidget(new QLabel(tr("Archive \"%1\" is protected. Enter password:").arg(archiveName)));

    m_edit = new QLineEdit;
    m_edit->setEchoMode(QLineEdit::Password);                        // 默认掩码显示
    layout->addWidget(m_edit);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, this, [this]() {
        m_cancelled = false;
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    // 宽度放大以容纳较长的提示信息
    const int baseWidth = sizeHint().width();
    if (baseWidth > 0)
        setMinimumWidth((baseWidth * kDialogWidthScaleNumerator) / kDialogWidthScaleDenominator);
}

QString PasswordDialog::password() const
{
    return m_edit->text();
}
