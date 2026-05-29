/** @file OperationResultDialog.cpp — 解压/压缩完成后的结果统计对话框 */

#include "OperationResultDialog.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QVBoxLayout>

OperationResultDialog::OperationResultDialog(Kind kind, const OperationStats &stats,
                                             const QString &detailPath, QWidget *parent)
    : QDialog(parent), m_kind(kind)
{
    constexpr int kDialogWidthScaleNumerator = 23;
    constexpr int kDialogWidthScaleDenominator = 10;

    // 根据操作类型设置标题
    setWindowTitle(kind == Kind::Extract ? tr("Extraction complete") : tr("Archive created"));
    setWindowFlag(Qt::WindowStaysOnTopHint, true);                   // 保持在顶层

    auto *layout = new QVBoxLayout(this);
    auto *form = new QFormLayout;

    // ── 各项统计 ──
    form->addRow(tr("Time elapsed:"), new QLabel(ArchiveUtils::formatDuration(stats.elapsedMs)));
    form->addRow(kind == Kind::Compress ? tr("Uncompressed size:") : tr("Archive size:"),
                 new QLabel(ArchiveUtils::formatBytes(stats.sizeBefore)));
    form->addRow(kind == Kind::Compress ? tr("Compressed size:") : tr("Extracted size:"),
                 new QLabel(ArchiveUtils::formatBytes(stats.sizeAfter)));
    form->addRow(tr("Size ratio:"),
                 new QLabel(ArchiveUtils::formatSizeRatio(stats.sizeBefore, stats.sizeAfter,
                                                          kind == Kind::Compress)));
    layout->addLayout(form);

    // 显示目标路径
    if (!detailPath.isEmpty()) {
        auto *pathLabel = new QLabel(detailPath);
        pathLabel->setWordWrap(true);
        pathLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);  // 允许复制路径
        layout->addWidget(pathLabel);
    }

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    layout->addWidget(buttons);

    // 宽度适度放大
    const int baseWidth = sizeHint().width();
    if (baseWidth > 0)
        setMinimumWidth((baseWidth * kDialogWidthScaleNumerator) / kDialogWidthScaleDenominator);
}
