/** @file ExtractDialog.cpp — 解压目标目录选择对话框 */

#include "ExtractDialog.h"

#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>

ExtractDialog::ExtractDialog(const QString &defaultDir, QWidget *parent) : QDialog(parent)
{
    setWindowTitle(tr("Extract"));

    auto *form = new QFormLayout;
    m_dirEdit = new QLineEdit(defaultDir);                           // 预填默认目录

    auto *browse = new QPushButton(tr("Browse…"));
    connect(browse, &QPushButton::clicked, this, [this]() {
        const QString dir = QFileDialog::getExistingDirectory(this, tr("Output folder"), m_dirEdit->text());
        if (!dir.isEmpty())
            m_dirEdit->setText(dir);
    });

    auto *row = new QHBoxLayout;
    row->addWidget(m_dirEdit, 1);                                    // 输入框占满剩余空间
    row->addWidget(browse);
    form->addRow(tr("To folder:"), row);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *outer = new QVBoxLayout(this);
    outer->addLayout(form);
    outer->addWidget(buttons);

    // 宽度加倍以容纳长路径
    const int baseWidth = sizeHint().width();
    const int targetWidth = baseWidth * 2;
    setMinimumWidth(targetWidth);
    resize(targetWidth, sizeHint().height());
}

QString ExtractDialog::outputDirectory() const { return m_dirEdit->text(); }
