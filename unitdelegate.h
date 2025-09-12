#ifndef UNITDELEGATE_H
#define UNITDELEGATE_H

#include <QStyledItemDelegate>

class UnitDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    explicit UnitDelegate(QObject *parent = nullptr);

    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option,
                          const QModelIndex &index) const override;

    void setEditorData(QWidget *editor, const QModelIndex &index) const override;
    void setModelData(QWidget *editor, QAbstractItemModel *model,
                      const QModelIndex &index) const override;

    void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option,
                              const QModelIndex &index) const override;
};

#endif // UNITDELEGATE_H
