#pragma once

#include <Eigen/Dense>
#include <complex>
#include <optional>
#include <string>
#include <istream>

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

} // namespace ts
