/** @file ArchiveModel.h — 压缩包条目列表的 Qt 表格模型 */

#pragma once

#include <QAbstractTableModel>
#include <QVector>

#include "SevenZipBackend.h"

class ArchiveModel : public QAbstractTableModel {
    Q_OBJECT
public:
    /// 表格列索引
    enum Column { Name = 0, Size, Packed, Modified, ColumnCount };

    explicit ArchiveModel(QObject *parent = nullptr);

    void setEntries(QVector<ArchiveEntry> entries);             // 替换全部条目
    const QVector<ArchiveEntry> &entries() const { return m_entries; }
    const ArchiveEntry *entryAt(int row) const;                 // 按行号取条目（越界返回 nullptr）

    // QAbstractTableModel 接口
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;

private:
    QVector<ArchiveEntry> m_entries;                            // 当前显示的条目列表
    static QString formatSize(qint64 bytes);                    // 格式化字节数为 B/KB/MB/GB
};
