/** @file ArchiveModel.cpp — 压缩包文件列表的 Qt 表格模型 */

#include "ArchiveModel.h"

#include <QLocale>

ArchiveModel::ArchiveModel(QObject *parent) : QAbstractTableModel(parent) {}

// ─── 数据管理 ────────────────────────────────────────────────

/** 替换整个条目列表（通知视图完全重置） */
void ArchiveModel::setEntries(QVector<ArchiveEntry> entries)
{
    beginResetModel();             // 通知视图准备重置
    m_entries = std::move(entries);
    endResetModel();               // 通知视图重置完成
}

/** 按行号获取条目指针，越界返回 nullptr */
const ArchiveEntry *ArchiveModel::entryAt(int row) const
{
    if (row < 0 || row >= m_entries.size())
        return nullptr;
    return &m_entries[row];
}

// ─── QAbstractTableModel 接口 ─────────────────────────────────

int ArchiveModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_entries.size();  // 仅顶层有行
}

int ArchiveModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : ColumnCount;        // Name, Size, Packed, Modified
}

// ─── 格式化 ──────────────────────────────────────────────────

/** 格式化字节数为可读字符串（B/KB/MB/GB） */
QString ArchiveModel::formatSize(qint64 bytes)
{
    static const QLocale locale;   // 缓存 QLocale 避免每次构造
    if (bytes < 0)
        return QString();
    if (bytes < 1024)
        return QString::number(bytes) + QStringLiteral(" B");
    if (bytes < 1024 * 1024)
        return locale.toString(bytes / 1024.0, 'f', 1) + QLatin1String(" KB");
    if (bytes < 1024LL * 1024 * 1024)
        return locale.toString(bytes / (1024.0 * 1024.0), 'f', 1) + QLatin1String(" MB");
    return locale.toString(bytes / (1024.0 * 1024.0 * 1024.0), 'f', 2) + QLatin1String(" GB");
}

// ─── 单元格数据 ──────────────────────────────────────────────

QVariant ArchiveModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || role != Qt::DisplayRole)
        return {};
    const ArchiveEntry &e = m_entries.at(index.row());
    switch (index.column()) {
    case Name:
        return e.path;
    case Size:
        return e.isDir ? QString() : formatSize(e.size);       // 目录不显示大小
    case Packed:
        return e.isDir ? QString() : formatSize(e.packedSize); // 目录不显示压缩后大小
    case Modified:
        return e.modified;
    default:
        return {};
    }
}

// ─── 表头 ────────────────────────────────────────────────────

QVariant ArchiveModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return {};
    switch (section) {
    case Name:
        return tr("Name");
    case Size:
        return tr("Size");
    case Packed:
        return tr("Packed");
    case Modified:
        return tr("Modified");
    default:
        return {};
    }
}
