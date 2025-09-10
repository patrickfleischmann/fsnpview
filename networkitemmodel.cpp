#include "networkitemmodel.h"
#include "network.h"
#include <QApplication>
#include <QIODevice>

NetworkItemModel::NetworkItemModel(QObject *parent)
    : QStandardItemModel(parent)
{
}

Qt::ItemFlags NetworkItemModel::flags(const QModelIndex &index) const
{
    Qt::ItemFlags defaultFlags = QStandardItemModel::flags(index);
    if (index.isValid()) {
        return Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled | defaultFlags;
    }
    return Qt::ItemIsDropEnabled | defaultFlags;
}

QStringList NetworkItemModel::mimeTypes() const
{
    QStringList types;
    types << "application/vnd.fsnpview.network";
    return types;
}

QMimeData *NetworkItemModel::mimeData(const QModelIndexList &indexes) const
{
    QMimeData *mimeData = new QMimeData();
    QByteArray encodedData;
    QDataStream stream(&encodedData, QIODevice::WriteOnly);

    for (const QModelIndex &index : indexes) {
        if (index.column() == 0) {
            // The pointer is stored in the first column's item
            QStandardItem *item = itemFromIndex(index.sibling(index.row(), 0));
            stream << item->data(Qt::UserRole).value<quintptr>();
        }
    }

    mimeData->setData("application/vnd.fsnpview.network", encodedData);
    return mimeData;
}

bool NetworkItemModel::canDropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent) const
{
    Q_UNUSED(action);
    Q_UNUSED(row);
    Q_UNUSED(column);
    Q_UNUSED(parent);
    return data->hasFormat("application/vnd.fsnpview.network");
}

bool NetworkItemModel::dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent)
{
    if (!canDropMimeData(data, action, row, column, parent)) {
        return false;
    }

    if (action == Qt::IgnoreAction) {
        return true;
    }

    QByteArray encodedData = data->data("application/vnd.fsnpview.network");
    QDataStream stream(&encodedData, QIODevice::ReadOnly);

    while (!stream.atEnd()) {
        quintptr network_ptr_val;
        stream >> network_ptr_val;
        Network *network = reinterpret_cast<Network*>(network_ptr_val);
        if (network) {
            emit networkDropped(network, parent);
        }
    }

    return true;
}
