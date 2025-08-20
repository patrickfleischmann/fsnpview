#include "parser_touchstone.h"
#include <Eigen/Dense>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <complex>
#include <fstream>
#include <optional>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>
#include <iostream>

namespace ts {

namespace detail {

struct OptionsLine {
    std::string freq_unit = "GHZ";
    std::string parameter = "S";
    std::string format = "MA";
    double R = 50.0;
};

std::string to_upper(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return std::toupper(c); });
    return s;
}

std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(s[b-1]))) --b;
    return s.substr(a, b - a);
}

std::string strip_comment(const std::string& s) {
    auto pos = s.find('!');
    return trim(pos == std::string::npos ? s : s.substr(0, pos));
}

double unit_scale_to_hz(const std::string& u_upper) {
    if (u_upper == "HZ") return 1.0;
    if (u_upper == "KHZ") return 1e3;
    if (u_upper == "MHZ") return 1e6;
    if (u_upper == "GHZ") return 1e9;
    throw std::runtime_error("Unknown frequency unit: " + u_upper);
}

std::optional<int> infer_ports_from_extension(const std::string& path) {
    std::regex re(".*\\.s(\\d+)p(\\..*)?$", std::regex::icase);
    std::smatch m;
    if (std::regex_match(path, m, re)) {
        return std::stoi(m[1].str());
    }
    return std::nullopt;
}

OptionsLine parse_options_line(const std::string& raw) {
    OptionsLine opts;
    std::string s = to_upper(strip_comment(raw));
    if (s.empty() || s[0] != '#') {
        throw std::runtime_error("Options line must start with '#'");
    }
    std::istringstream iss(s.substr(1));
    std::string tok;
    std::vector<std::string> toks;
    while (iss >> tok) toks.push_back(tok);

    for (const auto& t : toks) {
        if (t == "HZ" || t == "KHZ" || t == "MHZ" || t == "GHZ") { opts.freq_unit = t; break; }
    }
    for (const auto& t : toks) {
        if (t == "S" || t == "Y" || t == "Z" || t == "H" || t == "G") { opts.parameter = t; break; }
    }
    for (const auto& t : toks) {
        if (t == "RI" || t == "MA" || t == "DB") { opts.format = t; break; }
    }
    for (size_t i = 0; i + 1 < toks.size(); ++i) {
        if (toks[i] == "R") {
            opts.R = std::stod(toks[i+1]);
            break;
        }
    }
    return opts;
}

std::vector<std::string> read_logical_lines(std::istream& in) {
    std::vector<std::string> lines;
    std::string line;
    std::string current;

    while (std::getline(in, line)) {
        if (!line.empty() && (line.back() == '\r')) line.pop_back();
        std::string stripped = strip_comment(line);
        if (stripped.empty()) continue;
        if (!stripped.empty() && stripped[0] == '+') {
            if (current.empty()) {
                current = trim(stripped.substr(1));
            } else {
                current += ' ';
                current += trim(stripped.substr(1));
            }
            continue;
        }
        if (!current.empty()) {
            lines.push_back(trim(current));
            current.clear();
        }
        current = trim(stripped);
    }
    if (!current.empty()) lines.push_back(trim(current));
    return lines;
}

std::vector<double> tokenize_numbers(const std::string& s) {
    std::vector<double> out;
    std::istringstream iss(s);
    double v;
    while (iss >> v) out.push_back(v);
    return out;
}

std::complex<double> pair_to_complex(double a, double b, const std::string& fmt_upper) {
    if (fmt_upper == "RI") {
        return {a, b};
    } else if (fmt_upper == "MA") {
        double mag = a;
        double ang_rad = b * M_PI / 180.0;
        return std::polar(mag, ang_rad);
    } else if (fmt_upper == "DB") {
        double mag = std::pow(10.0, a / 20.0);
        double ang_rad = b * M_PI / 180.0;
        return std::polar(mag, ang_rad);
    }
    throw std::runtime_error("Unsupported format: " + fmt_upper);
}

} // namespace detail

TouchstoneData parse_touchstone_stream(std::istream& in, const std::string& source_name, std::optional<int> ports_hint) {
    using namespace detail;
    auto logical = read_logical_lines(in);
    if (logical.empty()) throw std::runtime_error("Empty Touchstone file: " + source_name);

    OptionsLine opts;
    size_t opt_idx = std::string::npos;
    for (size_t i = 0; i < logical.size(); ++i) {
        if (!logical[i].empty() && logical[i][0] == '#') { opt_idx = i; break; }
    }
    if (opt_idx != std::string::npos) {
        opts = parse_options_line(logical[opt_idx]);
    }

    std::vector<std::vector<double>> rows;
    rows.reserve(logical.size());
    for (size_t i = (opt_idx == std::string::npos ? 0 : opt_idx + 1); i < logical.size(); ++i) {
        const auto nums = tokenize_numbers(logical[i]);
        if (!nums.empty()) rows.push_back(nums);
    }
    if (rows.empty()) throw std::runtime_error("No numeric data rows found in: " + source_name);

    int P = 0;
    if (ports_hint && *ports_hint > 0) {
        P = *ports_hint;
    } else {
        const size_t cols = rows.front().size();
        if (cols < 1 + 2) throw std::runtime_error("Data row too short to infer port count in: " + source_name);
        bool inferred = false;
        for (int p = 1; p <= 128; ++p) {
            if (cols == 1 + 2ull * p * p) { P = p; inferred = true; break; }
        }
        if (!inferred) {
            throw std::runtime_error("Could not infer port count from column count (" + std::to_string(cols) + ") in: " + source_name);
        }
    }

    const size_t N = rows.size();
    TouchstoneData out;
    out.ports = P;
    out.parameter = opts.parameter;
    out.format = opts.format;
    out.freq_unit = opts.freq_unit;
    out.R = opts.R;

    out.freq.resize(static_cast<Eigen::Index>(N));
    out.sparams.resize(static_cast<Eigen::Index>(N), static_cast<Eigen::Index>(P*P));

    const double scale = unit_scale_to_hz(opts.freq_unit);

    for (size_t k = 0; k < N; ++k) {
        const auto& row = rows[k];
        const size_t expected_cols = 1 + 2ull * P * P;
        if (row.size() != expected_cols) {
            throw std::runtime_error(
                "Row " + std::to_string(k) + ": expected " + std::to_string(expected_cols) +
                " numeric columns (freq + pairs), got " + std::to_string(row.size())
                );
        }
        const double f_user = row[0];
        out.freq[static_cast<Eigen::Index>(k)] = f_user * scale;

        for (int m = 0; m < P * P; ++m) {
            double a = row[1 + 2*m + 0];
            double b = row[1 + 2*m + 1];
            std::complex<double> c = pair_to_complex(a, b, opts.format);
            out.sparams(static_cast<Eigen::Index>(k), m) = c;
        }
    }

    return out;
}

TouchstoneData parse_touchstone(const std::string& path) {
    using namespace detail;
    std::ifstream fin(path);
    std::cout << "Hello, path is:" << path << std::endl;


    if (!fin) throw std::runtime_error("Failed to open Touchstone file: " + path);
    std::optional<int> hint = infer_ports_from_extension(path);
    return parse_touchstone_stream(fin, path, hint);
}

std::complex<double> get_sparam(const TouchstoneData& data, Eigen::Index k, int i, int j) {
    return data.sparams(k, i*data.ports + j);
}

} // namespace ts
