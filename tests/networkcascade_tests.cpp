#include "networkcascade.h"
#include "networkfile.h"
#include <Eigen/Dense>
#include <cassert>
#include <complex>
#include <iostream>

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

int main()
{
    test_cascade_two_files();
    std::cout << "All NetworkCascade tests passed." << std::endl;
    return 0;
}

