/** @file DialogUtils.h — 对话框居中和宽度放大的工具函数 */

#pragma once

#include <QDialog>
#include <QMessageBox>

namespace DialogUtils {

    /** 将弹窗居中于参考窗口（reference 为空时依次尝试 parent → active window → screen） */
    void centerPopup(QWidget *popup, QWidget *reference = nullptr);

    /** 居中后以 exec() 模态运行 */
    int execCentered(QDialog &dialog, QWidget *reference = nullptr);

    /** 显示居中的 QMessageBox；额外施加 30% 宽度放大 */
    QMessageBox::StandardButton showCenteredMessageBox(
        QWidget *parent, QMessageBox::Icon icon, const QString &title, const QString &text,
        QMessageBox::StandardButtons buttons = QMessageBox::Ok,
        QMessageBox::StandardButton defaultButton = QMessageBox::NoButton);

} // namespace DialogUtils
