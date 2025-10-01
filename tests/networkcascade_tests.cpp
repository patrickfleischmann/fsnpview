#include "networkcascade.h"
#include "networkfile.h"
#include "networklumped.h"
#include <Eigen/Dense>
#include <cassert>
#include <complex>
#include <iostream>
#include <cmath>

void test_cascade_two_files()
{
    NetworkFile net1(QStringLiteral("test/a (1).s2p"));
    NetworkFile net2(QStringLiteral("test/a (2).s2p"));

    NetworkCascade cascade;
    cascade.addNetwork(&net1);
    cascade.addNetwork(&net2);

    Eigen::VectorXd freq(1);
    freq << 40e6; // 40 MHz

    Eigen::MatrixXcd s_matrix = cascade.sparameters(freq);

    std::complex<double> s11_expected(0.014920405958677847, 0.01630490315700499);
    std::complex<double> s12_expected(0.9574040676271609, -0.20781677254865813);
    std::complex<double> s21_expected(0.959119351068381, -0.2029186101162592);
    std::complex<double> s22_expected(0.01709181671098506, 0.014520122008960062);

    auto close = [](std::complex<double> a, std::complex<double> b) {
        return std::abs(a.real() - b.real()) < 1e-6 &&
               std::abs(a.imag() - b.imag()) < 1e-6;
    };

    assert(close(s_matrix(0, 0), s11_expected));
    assert(close(s_matrix(0, 1), s12_expected));
    assert(close(s_matrix(0, 2), s21_expected));
    assert(close(s_matrix(0, 3), s22_expected));
}

void test_wrap_to_minus_pi_pi()
{
    Eigen::ArrayXd values(5);
    values << M_PI, -M_PI, M_PI + 1e-10, -M_PI - 1e-10, M_PI - 1e-10;
    Eigen::ArrayXd wrapped = Network::wrapToMinusPiPi(values);

    constexpr double pi = M_PI;
    constexpr double tol = 1e-9;

    auto inRange = [&](double angle) {
        return angle <= pi + tol && angle >= -pi - tol;
    };

    assert(std::abs(wrapped(0) + pi) <= tol);
    assert(std::abs(wrapped(1) + pi) <= tol);
    assert(std::abs(wrapped(2) + pi) <= tol);
    assert(std::abs(wrapped(3) + pi) <= tol);
    assert(std::abs(wrapped(4) - (pi - 1e-10)) <= tol);

    for (int i = 0; i < wrapped.size(); ++i)
        assert(inRange(wrapped(i)));
}

namespace {
constexpr double pi = 3.14159265358979323846;

Eigen::ArrayXd unwrap_degrees(const QVector<double>& phase_deg)
{
    if (phase_deg.isEmpty())
        return {};

    Eigen::ArrayXd phase_rad = Eigen::Map<const Eigen::ArrayXd>(phase_deg.constData(), phase_deg.size()) * (pi / 180.0);
    Eigen::ArrayXd unwrapped = phase_rad;
    for (int i = 1; i < unwrapped.size(); ++i) {
        double diff = unwrapped(i) - unwrapped(i - 1);
        if (diff > pi) {
            for (int j = i; j < unwrapped.size(); ++j)
                unwrapped(j) -= 2.0 * pi;
        } else if (diff < -pi) {
            for (int j = i; j < unwrapped.size(); ++j)
                unwrapped(j) += 2.0 * pi;
        }
    }
    return unwrapped * (180.0 / pi);
}
}

void test_phase_unwrap_toggle()
{
    NetworkFile net(QStringLiteral("test/a (1).s2p"));

    net.setUnwrapPhase(true);
    const auto unwrapped = net.getPlotData(0, PlotType::Phase);

    net.setUnwrapPhase(false);
    const auto wrapped = net.getPlotData(0, PlotType::Phase);

    assert(!wrapped.second.isEmpty());
    assert(unwrapped.second.size() == wrapped.second.size());

    bool differenceFound = false;
    for (int i = 0; i < wrapped.second.size(); ++i) {
        if (std::abs(unwrapped.second[i] - wrapped.second[i]) > 1e-3) {
            differenceFound = true;
            break;
        }
    }

    assert(differenceFound);
}


void test_lumped_frequency_point_count()
{
    NetworkLumped lumped(NetworkLumped::NetworkType::R_series, {50.0});
    lumped.setFmin(2e6);
    lumped.setFmax(12e6);
    lumped.setPointCount(11);

    const auto freqs = lumped.frequencies();
    assert(freqs.size() == 11);
    assert(std::abs(freqs.first() - 2e6) < 1e-6);
    assert(std::abs(freqs.last() - 12e6) < 1e-6);

    const auto plotData = lumped.getPlotData(0, PlotType::Magnitude);
    assert(plotData.first.size() == 11);
    assert(plotData.second.size() == 11);
}

void test_cascade_frequency_settings()
{
    NetworkCascade cascade;
    cascade.setFrequencyRange(1e6, 5e6);
    cascade.setPointCount(9);

    const auto freqs = cascade.frequencies();
    assert(freqs.size() == 9);
    assert(std::abs(freqs.first() - 1e6) < 1e-6);
    assert(std::abs(freqs.last() - 5e6) < 1e-6);
}

void test_cascade_manual_range_persistence()
{
    NetworkFile net(QStringLiteral("test/a (1).s2p"));
    NetworkFile net2(QStringLiteral("test/a (2).s2p"));

    NetworkCascade cascade;
    cascade.setFrequencyRange(1e6, 5e6);
    cascade.setPointCount(9);

    cascade.addNetwork(&net);
    cascade.addNetwork(&net2);

    const auto freqs = cascade.frequencies();
    assert(freqs.size() == 9);
    assert(std::abs(freqs.first() - 1e6) < 1e-6);
    assert(std::abs(freqs.last() - 5e6) < 1e-6);
}

void test_lumped_phase_unwrap_matches_manual()
{
    NetworkLumped transmissionLine(NetworkLumped::NetworkType::TransmissionLine,
                                   {100.0, 50.0, 2.5});

    transmissionLine.setUnwrapPhase(false);
    const auto wrapped = transmissionLine.getPlotData(0, PlotType::Phase);

    transmissionLine.setUnwrapPhase(true);
    const auto unwrapped = transmissionLine.getPlotData(0, PlotType::Phase);

    assert(unwrapped.second.size() == wrapped.second.size());
    Eigen::ArrayXd manual = unwrap_degrees(wrapped.second);

    bool differenceFound = false;
    for (int i = 0; i < wrapped.second.size(); ++i) {
        double expected = manual(i);
        double actual = unwrapped.second[i];
        assert(std::abs(actual - expected) <= 1e-6);
        if (std::abs(actual - wrapped.second[i]) > 1e-3)
            differenceFound = true;
    }

    assert(differenceFound);
}

void test_transmission_line_group_delay()
{
    NetworkLumped transmissionLine(NetworkLumped::NetworkType::TransmissionLine,
                                   {1000.0, 50.0, 1.0});
    transmissionLine.setFmin(1e6);
    transmissionLine.setFmax(10e6);
    transmissionLine.setPointCount(11);

    const auto groupDelay = transmissionLine.getPlotData(1, PlotType::GroupDelay);
    assert(!groupDelay.first.isEmpty());
    assert(groupDelay.first.size() == groupDelay.second.size());

    constexpr double c0 = 299792458.0;
    const double expectedDelay = 1.0 / c0;

    for (double value : groupDelay.second) {
        assert(std::abs(value - expectedDelay) < 1e-12);
    }
}

void test_cascade_group_delay_adds_line_lengths()
{
    NetworkLumped line1(NetworkLumped::NetworkType::TransmissionLine,
                        {500.0, 50.0, 1.0});
    NetworkLumped line2(NetworkLumped::NetworkType::TransmissionLine,
                        {750.0, 50.0, 1.0});

    line1.setFmin(1e6);
    line1.setFmax(10e6);
    line1.setPointCount(11);
    line2.setFmin(1e6);
    line2.setFmax(10e6);
    line2.setPointCount(11);

    NetworkCascade cascade;
    cascade.setFrequencyRange(1e6, 10e6);
    cascade.setPointCount(11);
    cascade.addNetwork(&line1);
    cascade.addNetwork(&line2);

    const auto cascadeDelay = cascade.getPlotData(1, PlotType::GroupDelay);
    assert(!cascadeDelay.first.isEmpty());
    assert(cascadeDelay.first.size() == cascadeDelay.second.size());

    constexpr double c0 = 299792458.0;
    const double expectedDelay = (0.5 + 0.75) / c0;

    for (double value : cascadeDelay.second) {
        assert(std::abs(value - expectedDelay) < 1e-12);
    }
}

void test_cascade_phase_unwrap_matches_manual()
{
    NetworkLumped section1(NetworkLumped::NetworkType::TransmissionLine, {50.0, 50.0, 3.0});
    NetworkLumped section2(NetworkLumped::NetworkType::TransmissionLine, {75.0, 50.0, 2.5});

    NetworkCascade cascade;
    cascade.addNetwork(&section1);
    cascade.addNetwork(&section2);

    cascade.setUnwrapPhase(false);
    const auto wrapped = cascade.getPlotData(0, PlotType::Phase);

    cascade.setUnwrapPhase(true);
    const auto unwrapped = cascade.getPlotData(0, PlotType::Phase);

    assert(unwrapped.second.size() == wrapped.second.size());
    Eigen::ArrayXd manual = unwrap_degrees(wrapped.second);

    bool differenceFound = false;
    for (int i = 0; i < wrapped.second.size(); ++i) {
        double expected = manual(i);
        double actual = unwrapped.second[i];
        assert(std::abs(actual - expected) <= 1e-6);
        if (std::abs(actual - wrapped.second[i]) > 1e-3)
            differenceFound = true;
    }

    assert(differenceFound);
}

int main()
{
    test_wrap_to_minus_pi_pi();
    test_cascade_two_files();
    test_phase_unwrap_toggle();
    test_lumped_frequency_point_count();
    test_cascade_frequency_settings();
    test_cascade_manual_range_persistence();
    test_lumped_phase_unwrap_matches_manual();
    test_transmission_line_group_delay();
    test_cascade_group_delay_adds_line_lengths();
    test_cascade_phase_unwrap_matches_manual();
    std::cout << "All NetworkCascade tests passed." << std::endl;
    return 0;
}

