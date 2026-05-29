/** @file OperationResultDialog.h — 操作完成后的结果统计对话框 */

#pragma once

#include <QDialog>

#include "utils/ArchiveUtils.h"

class OperationResultDialog : public QDialog {
    Q_OBJECT
public:
    enum class Kind { Extract, Compress };               // 操作类型

    explicit OperationResultDialog(Kind kind, const OperationStats &stats, const QString &detailPath,
                                   QWidget *parent = nullptr);

private:
    Kind m_kind;
};
