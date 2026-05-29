/** @file AddDialog.h — 创建压缩包的参数设置对话框 */

#pragma once

#include <QDialog>
#include <optional>

class QCheckBox;
class QComboBox;
class QLineEdit;

/** 压缩包创建选项 */
struct AddArchiveOptions {
    QString archivePath;     // 目标压缩包路径
    QString format;          // 压缩格式 (7z, zip, tar, gzip, bzip2, xz, wim)
    int level = 5;           // 压缩级别 0–9（7z -mx 参数）
    QString password;        // 加密密码，空 = 不加密
};

class AddDialog : public QDialog {
    Q_OBJECT
public:
    explicit AddDialog(const QString &defaultArchivePath, QWidget *parent = nullptr);

    /** 模态运行对话框；用户确认后返回填充的选项，取消返回 nullopt */
    std::optional<AddArchiveOptions> options();

private slots:
    void onFormatChanged(const QString &format);          // 格式切换时自动更新扩展名
    void browseSavePath();                                // 弹出文件保存对话框
    void validateAndAccept();                             // 验证密码一致性和路径非空

private:
    void updateExtensionForFormat(const QString &format); // 根据格式替换路径扩展名

    QLineEdit *m_pathEdit = nullptr;
    QComboBox *m_formatCombo = nullptr;
    QComboBox *m_levelCombo = nullptr;
    QLineEdit *m_passwordEdit = nullptr;
    QLineEdit *m_confirmEdit = nullptr;
};
