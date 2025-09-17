#include "parser_touchstone.h"

#include <Eigen/Dense>

#include <algorithm>
#include <cerrno>
#include <cctype>
#include <cmath>
#include <complex>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include <deque>

namespace ts {

namespace detail {

struct OptionsLine {
    std::string freq_unit = "GHZ";
    std::string parameter = "S";
    std::string format = "MA";
    double R = 50.0;
};

std::string to_upper(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return s;
}

std::string trim(const std::string& s) {
    std::size_t a = 0;
    std::size_t b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) {
        ++a;
    }
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) {
        --b;
    }
    return s.substr(a, b - a);
}

std::string strip_comment(const std::string& s) {
    const std::size_t pos = s.find('!');
    const std::string without_comment = pos == std::string::npos ? s : s.substr(0, pos);
    return trim(without_comment);
}

constexpr double kPi = 3.141592653589793238462643383279502884;
constexpr double kDegToRad = kPi / 180.0;
constexpr double kRadToDeg = 180.0 / kPi;

double unit_scale_to_hz(const std::string& u_upper) {
    if (u_upper == "HZ") {
        return 1.0;
    }
    if (u_upper == "KHZ") {
        return 1e3;
    }
    if (u_upper == "MHZ") {
        return 1e6;
    }
    if (u_upper == "GHZ") {
        return 1e9;
    }
    throw std::runtime_error("Unknown frequency unit: " + u_upper);
}

double hz_to_unit_scale(const std::string& u_upper) {
    const double scale = unit_scale_to_hz(u_upper);
    if (scale == 0.0) {
        throw std::runtime_error("Invalid frequency scale for unit: " + u_upper);
    }
    return 1.0 / scale;
}

std::optional<int> infer_ports_from_extension(const std::string& path) {
    if (path.empty()) {
        return std::nullopt;
    }
    std::string lower;
    lower.reserve(path.size());
    for (unsigned char c : path) {
        lower.push_back(static_cast<char>(std::tolower(c)));
    }
    const std::size_t marker = lower.rfind(".s");
    if (marker == std::string::npos || marker + 2 >= lower.size()) {
        return std::nullopt;
    }
    std::size_t digit_begin = marker + 2;
    std::size_t digit_end = digit_begin;
    while (digit_end < lower.size() && std::isdigit(static_cast<unsigned char>(lower[digit_end]))) {
        ++digit_end;
    }
    if (digit_end == digit_begin) {
        return std::nullopt;
    }
    if (digit_end >= lower.size() || lower[digit_end] != 'p') {
        return std::nullopt;
    }
    try {
        const int ports = std::stoi(lower.substr(digit_begin, digit_end - digit_begin));
        if (ports > 0) {
            return ports;
        }
    } catch (const std::exception&) {
        return std::nullopt;
    }
    return std::nullopt;
}

OptionsLine parse_options_line(const std::string& raw) {
    OptionsLine opts;
    const std::string s = to_upper(strip_comment(raw));
    if (s.empty() || s[0] != '#') {
        throw std::runtime_error("Options line must start with '#'");
    }
    std::istringstream iss(s.substr(1));
    std::string tok;
    std::vector<std::string> toks;
    while (iss >> tok) {
        toks.push_back(tok);
    }

    for (const std::string& t : toks) {
        if (t == "HZ" || t == "KHZ" || t == "MHZ" || t == "GHZ") {
            opts.freq_unit = t;
            break;
        }
    }
    for (const std::string& t : toks) {
        if (t == "S" || t == "Y" || t == "Z" || t == "H" || t == "G") {
            opts.parameter = t;
            break;
        }
    }
    for (const std::string& t : toks) {
        if (t == "RI" || t == "MA" || t == "DB") {
            opts.format = t;
            break;
        }
    }
    for (std::size_t i = 0; i + 1 < toks.size(); ++i) {
        if (toks[i] == "R") {
            try {
                opts.R = std::stod(toks[i + 1]);
            } catch (const std::exception&) {
                throw std::runtime_error("Invalid reference impedance value in options line: " + raw);
            }
            break;
        }
    }
    return opts;
}

std::vector<double> tokenize_numbers(const std::string& s) {
    std::vector<double> out;
    const char* ptr = s.c_str();
    char* end = nullptr;
    errno = 0;
    while (*ptr != '\0') {
        const double value = std::strtod(ptr, &end);
        if (end == ptr) {
            while (*ptr != '\0' && std::isspace(static_cast<unsigned char>(*ptr))) {
                ++ptr;
            }
            if (*ptr != '\0') {
                throw std::runtime_error("Invalid numeric token in Touchstone data row: '" + s + "'");
            }
            break;
        }
        if (errno == ERANGE) {
            throw std::runtime_error("Numeric value out of range in Touchstone data row: '" + s + "'");
        }
        out.push_back(value);
        ptr = end;
        errno = 0;
    }
    return out;
}

std::complex<double> pair_to_complex(double a, double b, const std::string& fmt_upper) {
    if (fmt_upper == "RI") {
        return {a, b};
    }
    if (fmt_upper == "MA") {
        return std::polar(a, b * kDegToRad);
    }
    if (fmt_upper == "DB") {
        const double mag = std::pow(10.0, a / 20.0);
        return std::polar(mag, b * kDegToRad);
    }
    throw std::runtime_error("Unsupported format: " + fmt_upper);
}

std::pair<double, double> complex_to_pair(const std::complex<double>& value, const std::string& fmt_upper) {
    if (fmt_upper == "RI") {
        return {std::real(value), std::imag(value)};
    }
    const double magnitude = std::abs(value);
    const double angle_deg = std::arg(value) * kRadToDeg;
    if (fmt_upper == "MA") {
        return {magnitude, angle_deg};
    }
    if (fmt_upper == "DB") {
        const double magnitude_db = 20.0 * std::log10(magnitude);
        return {magnitude_db, angle_deg};
    }
    throw std::runtime_error("Unsupported format: " + fmt_upper);
}

} // namespace detail

TouchstoneData parse_touchstone_stream(std::istream& in, const std::string& source_name, std::optional<int> ports_hint) {
    using namespace detail;

    OptionsLine opts;
    double freq_scale = unit_scale_to_hz(opts.freq_unit);

    int ports = ports_hint && *ports_hint > 0 ? *ports_hint : 0;
    std::size_t values_per_row = ports > 0 ? static_cast<std::size_t>(ports) * static_cast<std::size_t>(ports) : 0;
    std::size_t expected_cols = values_per_row > 0 ? 1 + 2ull * values_per_row : 0;

    std::vector<double> frequencies_hz;
    std::vector<std::complex<double>> sparams_values;

    std::deque<double> pending_numbers;
    std::size_t pending_line_number = 0;

    const auto flush_pending_numbers = [&](std::size_t line_number, bool final_flush) {
        if (pending_numbers.empty()) {
            return;
        }

        if (ports <= 0) {
            constexpr int kMaxPortsToInfer = 128;
            int matched_ports = 0;
            for (int candidate = 1; candidate <= kMaxPortsToInfer; ++candidate) {
                const std::size_t candidate_cols = 1 + 2ull * static_cast<std::size_t>(candidate) * static_cast<std::size_t>(candidate);
                if (pending_numbers.size() >= candidate_cols && (pending_numbers.size() % candidate_cols == 0 || final_flush)) {
                    matched_ports = candidate;
                }
            }
            if (matched_ports > 0) {
                ports = matched_ports;
                values_per_row = static_cast<std::size_t>(ports) * static_cast<std::size_t>(ports);
                expected_cols = 1 + 2ull * values_per_row;
            } else {
                if (final_flush) {
                    const std::size_t line_ref = pending_line_number != 0 ? pending_line_number : line_number;
                    throw std::runtime_error("Could not infer port count from data near line " + std::to_string(line_ref) + " in " + source_name);
                }
                return;
            }
        } else if (expected_cols == 0) {
            values_per_row = static_cast<std::size_t>(ports) * static_cast<std::size_t>(ports);
            expected_cols = 1 + 2ull * values_per_row;
        }

        if (expected_cols == 0) {
            return;
        }

        while (pending_numbers.size() >= expected_cols) {
            std::vector<double> numbers;
            numbers.reserve(expected_cols);
            for (std::size_t idx = 0; idx < expected_cols; ++idx) {
                numbers.push_back(pending_numbers.front());
                pending_numbers.pop_front();
            }

            frequencies_hz.push_back(numbers[0] * freq_scale);
            for (std::size_t idx = 0; idx < values_per_row; ++idx) {
                const double a = numbers[1 + idx * 2];
                const double b = numbers[1 + idx * 2 + 1];
                sparams_values.push_back(pair_to_complex(a, b, opts.format));
            }
            pending_line_number = line_number;
        }

        if (pending_numbers.empty()) {
            pending_line_number = 0;
        } else if (final_flush) {
            const std::size_t line_ref = pending_line_number != 0 ? pending_line_number : line_number;
            throw std::runtime_error("Row starting near line " + std::to_string(line_ref) + " in " + source_name + " is incomplete");
        }
    };

    std::string physical_line;
    std::size_t physical_line_number = 0;
    while (std::getline(in, physical_line)) {
        ++physical_line_number;
        if (!physical_line.empty() && physical_line.back() == '\r') {
            physical_line.pop_back();
        }

        const std::size_t excl_pos = physical_line.find('!');
        const std::string without_comment = excl_pos == std::string::npos ? physical_line : physical_line.substr(0, excl_pos);
        std::string trimmed = trim(without_comment);
        if (trimmed.empty()) {
            continue;
        }

        if (trimmed[0] == '#') {
            flush_pending_numbers(physical_line_number, false);
            if (!pending_numbers.empty()) {
                const std::size_t line_ref = pending_line_number != 0 ? pending_line_number : physical_line_number;
                throw std::runtime_error("Dangling data before options line near line " + std::to_string(line_ref) + " in " + source_name);
            }
            opts = parse_options_line(trimmed);
            freq_scale = unit_scale_to_hz(opts.freq_unit);
            continue;
        }

        if (trimmed[0] == '+' && trimmed.size() > 1 && std::isspace(static_cast<unsigned char>(trimmed[1]))) {
            trimmed.erase(trimmed.begin());
            trimmed = trim(trimmed);
        }

        const std::vector<double> numbers = tokenize_numbers(trimmed);
        if (numbers.empty()) {
            continue;
        }

        if (pending_numbers.empty()) {
            pending_line_number = physical_line_number;
        }

        for (double value : numbers) {
            pending_numbers.push_back(value);
        }

        flush_pending_numbers(physical_line_number, false);
    }

    flush_pending_numbers(physical_line_number, true);

    if (frequencies_hz.empty()) {
        throw std::runtime_error("No numeric data rows found in: " + source_name);
    }

    if (ports <= 0) {
        throw std::runtime_error("Failed to determine port count for: " + source_name);
    }

    const std::size_t values_expected = frequencies_hz.size() * (static_cast<std::size_t>(ports) * static_cast<std::size_t>(ports));
    if (sparams_values.size() != values_expected) {
        throw std::runtime_error("Data size mismatch while parsing Touchstone file: " + source_name);
    }

    TouchstoneData out;
    out.ports = ports;
    out.parameter = opts.parameter;
    out.format = opts.format;
    out.freq_unit = opts.freq_unit;
    out.R = opts.R;

    const Eigen::Index row_count = static_cast<Eigen::Index>(frequencies_hz.size());
    const Eigen::Index col_count = static_cast<Eigen::Index>(ports * ports);

    out.freq.resize(row_count);
    out.sparams.resize(row_count, col_count);

    for (Eigen::Index row = 0; row < row_count; ++row) {
        out.freq[row] = frequencies_hz[static_cast<std::size_t>(row)];
        for (Eigen::Index col = 0; col < col_count; ++col) {
            const std::size_t flat_index = static_cast<std::size_t>(row) * static_cast<std::size_t>(col_count) + static_cast<std::size_t>(col);
            out.sparams(row, col) = sparams_values[flat_index];
        }
    }

    return out;
}

TouchstoneData parse_touchstone(const std::string& path) {
    std::ifstream fin(path);
    if (!fin) {
        throw std::runtime_error("Failed to open Touchstone file: " + path);
    }
    const std::optional<int> hint = detail::infer_ports_from_extension(path);
    return parse_touchstone_stream(fin, path, hint);
}

std::complex<double> get_sparam(const TouchstoneData& data, Eigen::Index k, int i, int j) {
    return data.sparams(k, j * data.ports + i);
}

void write_touchstone_stream(const TouchstoneData& data, std::ostream& out) {
    using namespace detail;

    if (data.ports <= 0) {
        throw std::runtime_error("Touchstone data must have a positive number of ports to be written");
    }

    const Eigen::Index expected_cols = static_cast<Eigen::Index>(data.ports) * static_cast<Eigen::Index>(data.ports);
    if (data.sparams.cols() != expected_cols) {
        throw std::runtime_error("Touchstone data has unexpected number of columns for S-parameters");
    }
    if (data.sparams.rows() != data.freq.size()) {
        throw std::runtime_error("Frequency vector length and S-parameter rows do not match");
    }

    const std::string freq_unit_upper = data.freq_unit.empty() ? std::string("HZ") : to_upper(data.freq_unit);
    const std::string parameter_upper = data.parameter.empty() ? std::string("S") : to_upper(data.parameter);
    const std::string format_upper = data.format.empty() ? std::string("RI") : to_upper(data.format);

    const double freq_scale = hz_to_unit_scale(freq_unit_upper);

    out << "# " << freq_unit_upper << ' ' << parameter_upper << ' ' << format_upper << " R " << data.R << '\n';

    const std::ostream::fmtflags original_flags = out.flags();
    const std::streamsize original_precision = out.precision();
    out.setf(std::ios::scientific);
    out.setf(std::ios::uppercase);
    out << std::setprecision(15);

    const Eigen::Index rows = data.sparams.rows();
    for (Eigen::Index row = 0; row < rows; ++row) {
        out << data.freq[row] * freq_scale;
        for (Eigen::Index col = 0; col < expected_cols; ++col) {
            const auto pair = complex_to_pair(data.sparams(row, col), format_upper);
            out << ' ' << pair.first << ' ' << pair.second;
        }
        out << '\n';
    }

    out.precision(original_precision);
    out.flags(original_flags);
}

void write_touchstone(const TouchstoneData& data, const std::string& path) {
    std::ofstream fout(path);
    if (!fout) {
        throw std::runtime_error("Failed to open Touchstone file for writing: " + path);
    }
    write_touchstone_stream(data, fout);
}

} // namespace ts
