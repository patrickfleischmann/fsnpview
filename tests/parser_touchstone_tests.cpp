#include "parser_touchstone.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <sstream>

void test_basic_parse() {
    ts::TouchstoneData data = ts::parse_touchstone("test/a (1).s2p");
    assert(data.ports == 2);
    assert(data.parameter == "S");
    assert(data.format == "DB");
    assert(data.freq_unit == "MHZ");
    assert(std::abs(data.freq[0] - 40e6) < 1e-3);

    std::complex<double> s11 = ts::get_sparam(data, 0, 0, 0);
    double expected_mag = std::pow(10.0, -33.163 / 20.0);
    double expected_ang_rad = 59.213 * M_PI / 180.0;
    std::complex<double> expected = std::polar(expected_mag, expected_ang_rad);
    assert(std::abs(std::real(s11) - std::real(expected)) < 1e-6);
    assert(std::abs(std::imag(s11) - std::imag(expected)) < 1e-6);

    std::complex<double> s21 = ts::get_sparam(data, 0, 1, 0);
    double expected_s21_mag = std::pow(10.0, -0.078 / 20.0);
    double expected_s21_ang_rad = -8.647 * M_PI / 180.0;
    std::complex<double> expected_s21 = std::polar(expected_s21_mag, expected_s21_ang_rad);
    assert(std::abs(std::real(s21) - std::real(expected_s21)) < 1e-6);
    assert(std::abs(std::imag(s21) - std::imag(expected_s21)) < 1e-6);
}

void test_second_file() {
    ts::TouchstoneData data = ts::parse_touchstone("test/a (2).s2p");
    assert(data.ports == 2);
    assert(data.freq.size() > 0);
}

void test_malformed() {
    std::istringstream ss("# Hz S RI R 50\n1.0 0.0\n");
    bool threw = false;
    try {
        ts::parse_touchstone_stream(ss, "<string>");
    } catch (const std::runtime_error&) {
        threw = true;
    }
    assert(threw);
}

void test_write_roundtrip() {
    ts::TouchstoneData original = ts::parse_touchstone("test/a (1).s2p");
    std::ostringstream oss;
    ts::write_touchstone_stream(original, oss);

    std::istringstream iss(oss.str());
    ts::TouchstoneData reparsed = ts::parse_touchstone_stream(iss, "<roundtrip>");

    assert(reparsed.ports == original.ports);
    assert(reparsed.freq.size() == original.freq.size());
    for (Eigen::Index i = 0; i < original.freq.size(); ++i) {
        assert(std::abs(reparsed.freq[i] - original.freq[i]) < 1e-6);
    }

    assert(reparsed.sparams.rows() == original.sparams.rows());
    assert(reparsed.sparams.cols() == original.sparams.cols());
    for (Eigen::Index r = 0; r < original.sparams.rows(); ++r) {
        for (Eigen::Index c = 0; c < original.sparams.cols(); ++c) {
            std::complex<double> a = original.sparams(r, c);
            std::complex<double> b = reparsed.sparams(r, c);
            assert(std::abs(a - b) < 1e-9);
        }
    }
}

void test_write_invalid_dimensions() {
    ts::TouchstoneData data;
    data.ports = 1;
    data.freq = Eigen::ArrayXd::Constant(1, 1.0e9);
    data.sparams = Eigen::ArrayXXcd::Zero(2, 1);

    std::ostringstream oss;
    bool threw = false;
    try {
        ts::write_touchstone_stream(data, oss);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    assert(threw);
}

void test_multiport_files() {
    ts::TouchstoneData data6 = ts::parse_touchstone("test/a (12).s6p");
    assert(data6.ports == 6);
    assert(data6.freq.size() > 0);
    assert(data6.sparams.cols() == data6.ports * data6.ports);

    ts::TouchstoneData data9 = ts::parse_touchstone("test/a (11).s9p");
    assert(data9.ports == 9);
    assert(data9.freq.size() > 0);
    assert(data9.sparams.cols() == data9.ports * data9.ports);
}

int main() {
    test_basic_parse();
    test_second_file();
    test_malformed();
    test_write_roundtrip();
    test_write_invalid_dimensions();
    test_multiport_files();
    std::cout << "All parser tests passed." << std::endl;
    return 0;
}

