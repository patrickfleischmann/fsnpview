#ifndef NETWORKITEMMODEL_H
#define NETWORKITEMMODEL_H

#include <QStandardItemModel>
#include <QMimeData>

class Network;

class NetworkItemModel : public QStandardItemModel
{
    Q_OBJECT
public:
    explicit NetworkItemModel(QObject *parent = nullptr);

    Qt::ItemFlags flags(const QModelIndex &index) const override;
    QStringList mimeTypes() const override;
    QMimeData *mimeData(const QModelIndexList &indexes) const override;
    bool canDropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent) const override;
    bool dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent) override;

signals:
    void networkDropped(Network* network, const QModelIndex& parent);
};

#endif // NETWORKITEMMODEL_H
