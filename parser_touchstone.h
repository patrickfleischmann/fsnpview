#pragma once

#include <Eigen/Dense>
#include <complex>
#include <istream>
#include <optional>
#include <ostream>
#include <string>

namespace ts {

struct TouchstoneData {
    int ports = 0;
    std::string parameter = "S";
    std::string format = "RI";
    std::string freq_unit = "HZ";
    double R = 50.0;

    Eigen::ArrayXd freq;
    Eigen::ArrayXXcd sparams;
};

TouchstoneData parse_touchstone_stream(std::istream& in, const std::string& source_name = "<istream>", std::optional<int> ports_hint = std::nullopt);

TouchstoneData parse_touchstone(const std::string& path);

std::complex<double> get_sparam(const TouchstoneData& data, Eigen::Index k, int i, int j);

void write_touchstone_stream(const TouchstoneData& data, std::ostream& out);

void write_touchstone(const TouchstoneData& data, const std::string& path);

} // namespace ts
