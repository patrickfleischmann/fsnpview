#ifndef PARAMETERSTYLEDIALOG_H
#define PARAMETERSTYLEDIALOG_H

#include <QDialog>
#include <QColor>
#include <QStringList>
#include <QPen>

class QComboBox;
class QPushButton;
class QDialogButtonBox;
class QFrame;
class Network;

class ParameterStyleDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ParameterStyleDialog(Network* network, QWidget* parent = nullptr);

    QString selectedParameter() const;
    QColor selectedColor() const;
    int selectedWidth() const;
    Qt::PenStyle selectedStyle() const;
    bool applyToAllParameters() const;

private slots:
    void chooseColor();
    void parameterChanged(int index);

private:
    void updateControlsForParameter(const QString& parameterKey);
    void updateColorPreview();

    Network* m_network;
    QStringList m_parameters;
    QColor m_color;

    QComboBox* m_parameterCombo;
    QComboBox* m_widthCombo;
    QComboBox* m_styleCombo;
    QPushButton* m_colorButton;
    QFrame* m_colorPreview;
    QDialogButtonBox* m_buttonBox;
};

#endif // PARAMETERSTYLEDIALOG_H
