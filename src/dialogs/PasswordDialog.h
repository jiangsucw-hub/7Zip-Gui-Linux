/** @file PasswordDialog.h — 密码输入对话框 */

#pragma once

#include <QDialog>

class QLineEdit;

class PasswordDialog : public QDialog {
    Q_OBJECT
public:
    explicit PasswordDialog(const QString &archiveName, QWidget *parent = nullptr);
    QString password() const;                            // 返回用户输入的密码
    bool cancelled() const { return m_cancelled; }       // 用户是否取消

private:
    QLineEdit *m_edit = nullptr;
    bool m_cancelled = true;                             // 默认视为取消
};
