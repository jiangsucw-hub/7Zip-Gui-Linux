/** @file ProgressDialog.cpp — 带进度条的模态进度对话框 */

#include "ProgressDialog.h"

#include <QLabel>
#include <QProgressBar>
#include <QVBoxLayout>

ProgressDialog::ProgressDialog(const QString &title, QWidget *parent) : QDialog(parent)
{
    setWindowTitle(title);
    setModal(true);                                                  // 阻止与其他窗口交互

    auto *layout = new QVBoxLayout(this);
    m_label = new QLabel;                                            // 显示当前阶段描述
    layout->addWidget(m_label);

    m_bar = new QProgressBar;
    m_bar->setRange(0, 100);                                         // 默认 0-100%
    layout->addWidget(m_bar);

    // 宽度放大 3 倍以容纳长标签文字
    const int baseWidth = sizeHint().width();
    setMinimumWidth(baseWidth * 3);
}

void ProgressDialog::setLabel(const QString &text) { m_label->setText(text); }

void ProgressDialog::setProgress(int percent) { m_bar->setValue(percent); }

/** 切换到不确定模式（动画滚动条）或确定模式（0-100%） */
void ProgressDialog::setIndeterminate(bool indeterminate)
{
    if (indeterminate)
        m_bar->setRange(0, 0);                                       // 0-0 = 不确定模式
    else
        m_bar->setRange(0, 100);
}
