#include "networkcascade.h"
#include "networkfile.h"
#include <Eigen/Dense>
#include <cassert>
#include <complex>
#include <iostream>
#include <cmath>

class DelayNetwork : public Network {
public:
    explicit DelayNetwork(double tau) : m_tau(tau) {
        m_fmin = 1e9;
        m_fmax = 2e9;
    }
    QString name() const override { return "DelayNetwork"; }
    Eigen::MatrixXcd abcd(const Eigen::VectorXd& freq) const override {
        Eigen::MatrixXcd matrix(freq.size(), 4);
        for (int i = 0; i < freq.size(); ++i) {
            double w = 2 * M_PI * freq(i);
            std::complex<double> s21 = std::exp(std::complex<double>(0, -w * m_tau));
            Eigen::Matrix2cd abcd_point = Network::s2abcd(0, s21, s21, 0);
            matrix.row(i) << abcd_point(0, 0), abcd_point(0, 1), abcd_point(1, 0), abcd_point(1, 1);
        }
        return matrix;
    }
    QPair<QVector<double>, QVector<double>> getPlotData(int, bool) override { return {}; }
private:
    double m_tau;
};

void test_cascade_two_files()
{
    NetworkFile net1(QStringLiteral("test/a (1).s2p"));
    NetworkFile net2(QStringLiteral("test/a (2).s2p"));

    NetworkCascade cascade;
    cascade.addNetwork(&net1);
    cascade.addNetwork(&net2);

    Eigen::VectorXd freq(1);
    freq << 40e6; // 40 MHz

    Eigen::MatrixXcd abcd_matrix = cascade.abcd(freq);
    Eigen::Matrix2cd abcd;
    abcd << abcd_matrix(0, 0), abcd_matrix(0, 1),
             abcd_matrix(0, 2), abcd_matrix(0, 3);

    Eigen::Vector4cd s = Network::abcd2s(abcd);

    std::complex<double> s11_expected(0.014920405958677847, 0.01630490315700499);
    std::complex<double> s12_expected(0.9574040676271609, -0.20781677254865813);
    std::complex<double> s21_expected(0.959119351068381, -0.2029186101162592);
    std::complex<double> s22_expected(0.01709181671098506, 0.014520122008960062);

    auto close = [](std::complex<double> a, std::complex<double> b) {
        return std::abs(a.real() - b.real()) < 1e-6 &&
               std::abs(a.imag() - b.imag()) < 1e-6;
    };

    assert(close(s[0], s11_expected));
    assert(close(s[1], s12_expected));
    assert(close(s[2], s21_expected));
    assert(close(s[3], s22_expected));
}

void test_phase_unwrap_cascade()
{
    DelayNetwork d1(1e-9);
    DelayNetwork d2(1e-9);

    NetworkCascade cascade;
    cascade.addNetwork(&d1);
    cascade.addNetwork(&d2);

    auto plotData = cascade.getPlotData(2, true); // s21 phase
    const auto& phases = plotData.second;

    assert(phases.size() >= 2);
    double first = phases[0];
    double last = phases[phases.size() - 1];

    assert(std::abs(first) < 1e-6);          // starts near 0 degrees
    assert(std::abs(last + 720.0) < 1e-6);   // ends around -720 degrees
}

int main()
{
    test_cascade_two_files();
    test_phase_unwrap_cascade();
    std::cout << "All NetworkCascade tests passed." << std::endl;
    return 0;
}

