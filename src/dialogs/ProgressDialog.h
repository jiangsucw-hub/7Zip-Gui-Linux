/** @file ProgressDialog.h — 模态进度对话框（确定 + 不确定模式） */

#pragma once

#include <QDialog>

class QLabel;
class QProgressBar;

class ProgressDialog : public QDialog {
    Q_OBJECT
public:
    explicit ProgressDialog(const QString &title, QWidget *parent = nullptr);
    void setLabel(const QString &text);                  // 设置阶段描述文字
    void setProgress(int percent);                       // 设置进度 0-100
    void setIndeterminate(bool indeterminate = true);    // 切换不确定模式（动画滚动条）

private:
    QLabel *m_label = nullptr;
    QProgressBar *m_bar = nullptr;
};
