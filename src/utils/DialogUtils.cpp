/** @file DialogUtils.cpp — 对话框居中与宽度放大的工具函数 */

#include "DialogUtils.h"

#include <algorithm>

#include <QApplication>
#include <QGuiApplication>
#include <QScreen>
#include <QWidget>

namespace {
constexpr int kDialogWidthScaleNumerator = 13;
constexpr int kDialogWidthScaleDenominator = 10;

/** 将弹窗宽度放大 30%，避免文字截断 */
void applyWiderDialogWidth(QWidget *popup)
{
    if (!popup)
        return;

    popup->adjustSize();
    const int baseWidth = popup->sizeHint().width();
    if (baseWidth <= 0)
        return;

    popup->setMinimumWidth((baseWidth * kDialogWidthScaleNumerator) / kDialogWidthScaleDenominator);
}

/**
 * 解析参考窗口的几何区域，用于确定居中基准
 * 优先级：传入的 reference → parent → active window → primary screen
 */
QRect resolveReferenceGeometry(QWidget *popup, QWidget *reference)
{
    if (reference && reference->isVisible())
        return reference->frameGeometry();

    QWidget *parent = popup ? popup->parentWidget() : nullptr;
    if (parent && parent->isVisible())
        return parent->frameGeometry();

    QWidget *active = QApplication::activeWindow();
    if (active && active->isVisible())
        return active->frameGeometry();

    QScreen *screen = popup ? popup->screen() : nullptr;
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    return screen ? screen->availableGeometry() : QRect(0, 0, 1280, 720);
}

/** 获取弹窗所在屏幕的可用区域 */
QRect resolveAvailableGeometry(QWidget *popup)
{
    QScreen *screen = popup ? popup->screen() : nullptr;
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    return screen ? screen->availableGeometry() : QRect(0, 0, 1280, 720);
}

} // namespace

namespace DialogUtils {

/**
 * 将弹窗居中于参考窗口
 * 如果超出屏幕边界则 clamp
 */
void centerPopup(QWidget *popup, QWidget *reference)
{
    if (!popup)
        return;

    popup->adjustSize();

    const QRect referenceGeometry = resolveReferenceGeometry(popup, reference);
    const QRect availableGeometry = resolveAvailableGeometry(popup);
    const QSize popupSize = popup->frameGeometry().size();

    // 计算居中位置
    QPoint topLeft(referenceGeometry.center().x() - popupSize.width() / 2,
                   referenceGeometry.center().y() - popupSize.height() / 2);

    // Clamp 到屏幕可用区域内
    const int minX = availableGeometry.left();
    const int maxX = availableGeometry.right() - popupSize.width() + 1;
    const int minY = availableGeometry.top();
    const int maxY = availableGeometry.bottom() - popupSize.height() + 1;

    topLeft.setX(std::clamp(topLeft.x(), minX, std::max(minX, maxX)));
    topLeft.setY(std::clamp(topLeft.y(), minY, std::max(minY, maxY)));

    popup->move(topLeft);
}

/** 居中后以模态方式运行对话框 */
int execCentered(QDialog &dialog, QWidget *reference)
{
    centerPopup(&dialog, reference);
    return dialog.exec();
}

/**
 * 显示居中的 QMessageBox
 * 额外施加 30% 宽度放大以容纳长文本
 */
QMessageBox::StandardButton showCenteredMessageBox(QWidget *parent, QMessageBox::Icon icon,
                                                   const QString &title, const QString &text,
                                                   QMessageBox::StandardButtons buttons,
                                                   QMessageBox::StandardButton defaultButton)
{
    QMessageBox box(icon, title, text, buttons, parent);
    if (defaultButton != QMessageBox::NoButton)
        box.setDefaultButton(defaultButton);
    applyWiderDialogWidth(&box);
    centerPopup(&box, parent);
    return static_cast<QMessageBox::StandardButton>(box.exec());
}

} // namespace DialogUtils
