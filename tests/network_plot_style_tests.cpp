#include "network.h"

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

void test_default_styles()
{
    DummyNetwork net;
    net.setColor(Qt::red);

    QPen s11Pen = net.parameterPen("s11");
    assert(s11Pen.color() == Qt::red);
    assert(s11Pen.width() == 0);
    assert(s11Pen.style() == Qt::DashLine);

    QPen s21Pen = net.parameterPen("s21");
    assert(s21Pen.color() == Qt::red);
    assert(s21Pen.width() == 0);
    assert(s21Pen.style() == Qt::SolidLine);
}

void test_custom_styles()
{
    DummyNetwork net;
    net.setColor(Qt::red);

    net.setParameterColor("s21", Qt::blue);
    net.setParameterWidth("s21", 4);
    net.setParameterStyle("s21", Qt::DotLine);

    QPen pen = net.parameterPen("s21");
    assert(pen.color() == Qt::blue);
    assert(pen.width() == 4);
    assert(pen.style() == Qt::DotLine);

    // Ensure values are clamped
    net.setParameterWidth("s21", 25);
    assert(net.parameterPen("s21").width() == 10);

    // Setting defaults removes overrides
    net.setParameterStyle("s21", Qt::SolidLine);
    assert(net.parameterPen("s21").style() == Qt::SolidLine);
}

void test_case_insensitivity()
{
    DummyNetwork net;
    net.setColor(Qt::green);
    assert(net.parameterPen("S11").style() == Qt::DashLine);
    assert(net.parameterPen("S21").style() == Qt::SolidLine);
}

int main()
{
    test_default_styles();
    test_custom_styles();
    test_case_insensitivity();
    std::cout << "All network style tests passed." << std::endl;
    return 0;
}
