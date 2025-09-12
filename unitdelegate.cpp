#include "unitdelegate.h"
#include "networklumped.h"
#include <QComboBox>

UnitDelegate::UnitDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
}

QWidget *UnitDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &option,
                                      const QModelIndex &index) const
{
    Q_UNUSED(option);
    Q_UNUSED(index);
    QComboBox *editor = new QComboBox(parent);
    editor->addItems({"Ohm", "F", "H", "f", "p", "n", "u", "m", "k", "M", "G"});
    return editor;
}

void UnitDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
    QString value = index.model()->data(index, Qt::EditRole).toString();
    QComboBox *comboBox = static_cast<QComboBox*>(editor);
    comboBox->setCurrentText(value);
}

void UnitDelegate::setModelData(QWidget *editor, QAbstractItemModel *model,
                                  const QModelIndex &index) const
{
    QComboBox *comboBox = static_cast<QComboBox*>(editor);
    QString value = comboBox->currentText();
    model->setData(index, value, Qt::EditRole);
}

void UnitDelegate::updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option,
                                          const QModelIndex &index) const
{
    Q_UNUSED(index);
    editor->setGeometry(option.rect);
}
