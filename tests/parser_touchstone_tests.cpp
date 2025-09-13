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

int main() {
    test_basic_parse();
    test_second_file();
    test_malformed();
    std::cout << "All parser tests passed." << std::endl;
    return 0;
}

