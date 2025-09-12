#include "networkitemmodel.h"
#include "network.h"
#include "networklumped.h"
#include <QApplication>
#include <QIODevice>
#include <QDebug>

NetworkItemModel::NetworkItemModel(QObject *parent)
    : QStandardItemModel(parent)
{
}

Qt::ItemFlags NetworkItemModel::flags(const QModelIndex &index) const
{
    Qt::ItemFlags defaultFlags = QStandardItemModel::flags(index);
    if (!index.isValid())
        return Qt::ItemIsDropEnabled | defaultFlags;

    QStandardItem *item = itemFromIndex(index.sibling(index.row(), 0));
    if (!item) return defaultFlags;

    Network *network = item->data(Qt::UserRole).value<Network*>();
    if (!network) return defaultFlags;

    if (dynamic_cast<NetworkLumped*>(network) && (index.column() == 1 || index.column() == 2)) {
        return Qt::ItemIsEditable | Qt::ItemIsSelectable | Qt::ItemIsEnabled;
    }

    return defaultFlags | Qt::ItemIsDragEnabled;
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

    QList<int> rows;
    for (const QModelIndex &index : indexes) {
        if (rows.contains(index.row())) {
            continue;
        }
        rows.append(index.row());

        QStandardItem *item = itemFromIndex(index.sibling(index.row(), 0));
        Network* network = item->data(Qt::UserRole).value<Network*>();
        stream.writeRawData(reinterpret_cast<const char*>(&network), sizeof(Network*));
    }

    mimeData->setData("application/vnd.fsnpview.network", encodedData);
    return mimeData;
}

bool NetworkItemModel::canDropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent) const
{
    Q_UNUSED(action);
    Q_UNUSED(column);
    Q_UNUSED(parent);

    if (!data->hasFormat("application/vnd.fsnpview.network")) {
        return false;
    }

    if (row != -1) { // dropping between items
        return true;
    }

    if (parent.isValid()) { // dropping on an item
        return true;
    }

    return true; // dropping in empty space
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

    QList<Network*> droppedNetworks;
    while (!stream.atEnd()) {
        Network* network_ptr;
        stream.readRawData(reinterpret_cast<char*>(&network_ptr), sizeof(Network*));
        droppedNetworks.append(network_ptr);
    }

    int dropRow = row;
    if (dropRow == -1) {
        if (parent.isValid()) {
            dropRow = parent.row();
        } else {
            dropRow = rowCount();
        }
    }

    for (Network* network : droppedNetworks) {
        emit networkDropped(network, index(dropRow, 0));
        dropRow++;
    }

    return true;
}

void NetworkItemModel::setColumnHeaders(const QStringList &headers)
{
    setHorizontalHeaderLabels(headers);
}
