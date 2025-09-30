#include "parameterstyledialog.h"
#include "network.h"

#include <QApplication>
#include <QComboBox>
#include <QColor>
#include <QPen>
#include <cassert>
#include <iostream>

class DummyNetwork : public Network
{
public:
    explicit DummyNetwork(int ports = 2)
        : Network(nullptr)
        , m_ports(ports)
    {
    }

    QString name() const override { return QStringLiteral("dummy"); }
    Eigen::MatrixXcd sparameters(const Eigen::VectorXd&) const override { return {}; }
    QPair<QVector<double>, QVector<double>> getPlotData(int, PlotType) override { return {}; }
    Network* clone(QObject* parent = nullptr) const override
    {
        auto* copy = new DummyNetwork(m_ports);
        copy->setColor(m_color);
        copy->setVisible(m_is_visible);
        copy->setUnwrapPhase(m_unwrap_phase);
        copy->setActive(m_is_active);
        copy->setFmin(m_fmin);
        copy->setFmax(m_fmax);
        copy->copyStyleSettingsFrom(this);
        return copy;
    }
    QVector<double> frequencies() const override { return {}; }
    int portCount() const override { return m_ports; }

private:
    int m_ports;
};

int main(int argc, char** argv)
{
    QApplication app(argc, argv);

    DummyNetwork net;
    net.setColor(Qt::red);

    ParameterStyleDialog dialog(&net);

    QObject* comboObject = dialog.findChild<QObject*>("widthCombo", Qt::FindChildrenRecursively);
    QComboBox* widthCombo = comboObject ? static_cast<QComboBox*>(comboObject) : nullptr;
    assert(widthCombo && "Failed to locate width combo box");

    const int desiredWidth = 4;
    widthCombo->setCurrentIndex(desiredWidth);

    assert(dialog.selectedWidth() == desiredWidth);

    const int desiredWidthAll = 7;
    widthCombo->setCurrentIndex(desiredWidthAll);
    assert(dialog.selectedWidth() == desiredWidthAll);

    std::cout << "Parameter style dialog tests passed." << std::endl;
    return 0;
}
