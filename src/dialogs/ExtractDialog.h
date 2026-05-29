/** @file ExtractDialog.h — 解压目标目录选择对话框 */

#pragma once

#include <QDialog>

class QLineEdit;

class ExtractDialog : public QDialog {
    Q_OBJECT
public:
    explicit ExtractDialog(const QString &defaultDir, QWidget *parent = nullptr);
    QString outputDirectory() const;                     // 返回用户选择的目录

private:
    QLineEdit *m_dirEdit = nullptr;                      // 目录路径输入框
};
