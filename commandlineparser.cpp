#include "commandlineparser.h"

#include <QLocale>
#include <QSet>
#include <QStringList>
#include <optional>

namespace {

QString normalizeToken(const QString& token)
{
    QString normalized;
    normalized.reserve(token.size());
    for (QChar c : token) {
        if (c.isLetterOrNumber()) {
            normalized.append(c.toLower());
        }
    }
    return normalized;
}

struct ParameterDefinition
{
    QString canonicalName;
    QStringList aliases;
    int index = -1;
    bool allowUnnamed = false;
};

struct LumpedDefinition
{
    QString canonicalName;
    QStringList aliases;
    NetworkLumped::NetworkType type;
    QVector<ParameterDefinition> parameters;
};

const QVector<LumpedDefinition>& lumpedDefinitions()
{
    static const QVector<LumpedDefinition> defs = {
        {QStringLiteral("R_series"), {QStringLiteral("RSeries"), QStringLiteral("RS")}, NetworkLumped::NetworkType::R_series,
         {{QStringLiteral("r"), {QStringLiteral("R"), QStringLiteral("Res")}, 0, true}}},
        {QStringLiteral("R_shunt"), {QStringLiteral("RShunt"), QStringLiteral("RP")}, NetworkLumped::NetworkType::R_shunt,
         {{QStringLiteral("r"), {QStringLiteral("R")}, 0, true}}},
        {QStringLiteral("C_series"), {QStringLiteral("CSeries")}, NetworkLumped::NetworkType::C_series,
         {{QStringLiteral("c"), {QStringLiteral("C")}, 0, true}}},
        {QStringLiteral("C_shunt"), {QStringLiteral("CShunt")}, NetworkLumped::NetworkType::C_shunt,
         {{QStringLiteral("c"), {QStringLiteral("C")}, 0, true}}},
        {QStringLiteral("L_series"), {QStringLiteral("LSeries")}, NetworkLumped::NetworkType::L_series,
         {{QStringLiteral("l"), {QStringLiteral("L"), QStringLiteral("Ind")}, 0, true},
          {QStringLiteral("rser"), {QStringLiteral("R_ser"), QStringLiteral("Rser")}, 1, true}}},
        {QStringLiteral("L_shunt"), {QStringLiteral("LShunt")}, NetworkLumped::NetworkType::L_shunt,
         {{QStringLiteral("l"), {QStringLiteral("L"), QStringLiteral("Ind")}, 0, true},
          {QStringLiteral("rser"), {QStringLiteral("R_ser"), QStringLiteral("Rser")}, 1, true}}},
        {QStringLiteral("TransmissionLine"), {QStringLiteral("TL"), QStringLiteral("TransLine")}, NetworkLumped::NetworkType::TransmissionLine,
         {{QStringLiteral("len"), {QStringLiteral("Len"), QStringLiteral("Length")}, 0, true},
          {QStringLiteral("z0"), {QStringLiteral("Z0")}, 1, true},
          {QStringLiteral("er_eff"), {QStringLiteral("Ereff"), QStringLiteral("EpsEff")}, 2, true}}},
        {QStringLiteral("TL_lossy"), {QStringLiteral("TransmissionLineLossy"), QStringLiteral("TLLossy")},
         NetworkLumped::NetworkType::TransmissionLineLossy,
         {{QStringLiteral("len"), {QStringLiteral("Len"), QStringLiteral("Length")}, 0, true},
          {QStringLiteral("z0"), {QStringLiteral("Z0")}, 1, true},
          {QStringLiteral("er_eff"), {QStringLiteral("Ereff"), QStringLiteral("EpsEff")}, 2, true},
          {QStringLiteral("a"), {QStringLiteral("Alpha"), QStringLiteral("Loss"), QStringLiteral("a_dBpm")}, 3, true},
          {QStringLiteral("a_d"), {QStringLiteral("AlphaD"), QStringLiteral("Ad"), QStringLiteral("a_d_dBpm")}, 4, true},
          {QStringLiteral("fa"), {QStringLiteral("Fa"), QStringLiteral("FreqRef")}, 5, true}}},
        {QStringLiteral("LRC_ser_shunt"), {QStringLiteral("LRCSerShunt")}, NetworkLumped::NetworkType::LRC_series_shunt,
         {{QStringLiteral("l"), {QStringLiteral("L"), QStringLiteral("Ind")}, 0, true},
          {QStringLiteral("r"), {QStringLiteral("R")}, 1, true},
          {QStringLiteral("c"), {QStringLiteral("C")}, 2, true}}},
        {QStringLiteral("LRC_par_ser"), {QStringLiteral("LRCParSer")}, NetworkLumped::NetworkType::LRC_parallel_series,
         {{QStringLiteral("l"), {QStringLiteral("L"), QStringLiteral("Ind")}, 0, true},
          {QStringLiteral("r"), {QStringLiteral("R")}, 1, true},
          {QStringLiteral("c"), {QStringLiteral("C")}, 2, true}}}
    };
    return defs;
}

const LumpedDefinition* findLumpedDefinition(const QString& token)
{
    const QString normalized = normalizeToken(token);
    for (const auto& def : lumpedDefinitions()) {
        if (normalizeToken(def.canonicalName) == normalized)
            return &def;
        for (const QString& alias : def.aliases) {
            if (normalizeToken(alias) == normalized)
                return &def;
        }
    }
    return nullptr;
}

const ParameterDefinition* findParameterDefinition(const LumpedDefinition& def, const QString& token)
{
    const QString normalized = normalizeToken(token);
    for (const auto& param : def.parameters) {
        if (normalizeToken(param.canonicalName) == normalized)
            return &param;
        for (const QString& alias : param.aliases) {
            if (normalizeToken(alias) == normalized)
                return &param;
        }
    }
    return nullptr;
}

bool parseDoubleToken(const QString& token, double& value)
{
    bool ok = false;
    value = QLocale::c().toDouble(token, &ok);
    return ok;
}

std::optional<int> nextUnnamedParameterIndex(const LumpedDefinition& def, const QSet<int>& assigned)
{
    for (const auto& param : def.parameters) {
        if (!param.allowUnnamed)
            continue;
        if (!assigned.contains(param.index))
            return param.index;
    }
    return std::nullopt;
}

bool parseLumpedParameters(const QStringList& args, int& index, const LumpedDefinition& def,
                           CommandLineParser::CascadeEntry& entry, QString& error)
{
    QSet<int> assigned;

    while (index < args.size()) {
        const QString token = args.at(index);
        if (token.startsWith('-'))
            break;
        if (findLumpedDefinition(token))
            break;

        const int eqPos = token.indexOf('=');
        if (eqPos > 0) {
            const QString namePart = token.left(eqPos);
            const QString valuePart = token.mid(eqPos + 1);
            const ParameterDefinition* param = findParameterDefinition(def, namePart);
            if (!param) {
                error = QStringLiteral("Unknown parameter '%1' for lumped network '%2'").arg(namePart, def.canonicalName);
                return false;
            }
            if (assigned.contains(param->index)) {
                error = QStringLiteral("Parameter '%1' for lumped network '%2' specified multiple times")
                            .arg(param->canonicalName, def.canonicalName);
                return false;
            }
            double value = 0.0;
            if (!parseDoubleToken(valuePart, value)) {
                error = QStringLiteral("Invalid numeric value '%1' for parameter '%2'").arg(valuePart, param->canonicalName);
                return false;
            }
            entry.parameterOverrides.append({param->index, value});
            assigned.insert(param->index);
            ++index;
            continue;
        }

        if (const ParameterDefinition* param = findParameterDefinition(def, token)) {
            if (assigned.contains(param->index)) {
                error = QStringLiteral("Parameter '%1' for lumped network '%2' specified multiple times")
                            .arg(param->canonicalName, def.canonicalName);
                return false;
            }
            if (index + 1 >= args.size()) {
                error = QStringLiteral("Missing value for parameter '%1' of lumped network '%2'")
                            .arg(param->canonicalName, def.canonicalName);
                return false;
            }
            const QString valueToken = args.at(index + 1);
            double value = 0.0;
            if (!parseDoubleToken(valueToken, value)) {
                error = QStringLiteral("Invalid numeric value '%1' for parameter '%2'").arg(valueToken, param->canonicalName);
                return false;
            }
            entry.parameterOverrides.append({param->index, value});
            assigned.insert(param->index);
            index += 2;
            continue;
        }

        double value = 0.0;
        if (parseDoubleToken(token, value)) {
            const std::optional<int> unnamedIndex = nextUnnamedParameterIndex(def, assigned);
            if (!unnamedIndex.has_value()) {
                error = QStringLiteral("Too many positional values for lumped network '%1'").arg(def.canonicalName);
                return false;
            }
            entry.parameterOverrides.append({unnamedIndex.value(), value});
            assigned.insert(unnamedIndex.value());
            ++index;
            continue;
        }

        break;
    }

    return true;
}

bool parseCascadeItems(const QStringList& args, int& index, CommandLineParser::Options& options, QString& error)
{
    bool parsedAny = false;
    while (index < args.size()) {
        const QString token = args.at(index);
        if (token.startsWith('-'))
            break;

        if (const auto* def = findLumpedDefinition(token)) {
            CommandLineParser::CascadeEntry entry;
            entry.type = CommandLineParser::CascadeEntry::Type::Lumped;
            entry.identifier = def->canonicalName;
            entry.lumpedType = def->type;
            ++index;
            if (!parseLumpedParameters(args, index, *def, entry, error))
                return false;
            options.cascade.append(entry);
            parsedAny = true;
            continue;
        }

        CommandLineParser::CascadeEntry entry;
        entry.type = CommandLineParser::CascadeEntry::Type::File;
        entry.identifier = token;
        options.cascade.append(entry);
        ++index;
        parsedAny = true;
    }

    if (!parsedAny) {
        error = QStringLiteral("Option -c/--cascade requires at least one network specification");
        return false;
    }

    options.cascadeRequested = true;
    return true;
}

} // namespace

CommandLineParser::ParseResult CommandLineParser::parse(int argc, char *argv[]) const
{
    ParseResult result;
    Options& options = result.options;
    options.argumentsProvided = argc > 1;

    QStringList args;
    args.reserve(argc - 1);
    for (int i = 1; i < argc; ++i) {
        args.append(QString::fromLocal8Bit(argv[i]));
    }

    bool treatAsPositional = false;
    for (int i = 0; i < args.size();) {
        const QString arg = args.at(i);

        if (!treatAsPositional && arg == QStringLiteral("--")) {
            treatAsPositional = true;
            ++i;
            continue;
        }

        if (!treatAsPositional && (arg == QStringLiteral("-h") || arg == QStringLiteral("--help"))) {
            options.helpRequested = true;
            ++i;
            continue;
        }

        if (!treatAsPositional && (arg == QStringLiteral("-n") || arg == QStringLiteral("--nogui"))) {
            options.noGui = true;
            ++i;
            continue;
        }

        if (!treatAsPositional && (arg == QStringLiteral("-s") || arg == QStringLiteral("--save"))) {
            if (i + 1 >= args.size()) {
                result.errorMessage = QStringLiteral("Option -s/--save requires a file path argument");
                return result;
            }
            options.saveRequested = true;
            options.savePath = args.at(i + 1);
            i += 2;
            continue;
        }

        if (!treatAsPositional && (arg == QStringLiteral("-f") || arg == QStringLiteral("--freq"))) {
            if (i + 3 >= args.size()) {
                result.errorMessage = QStringLiteral("Option -f/--freq requires three arguments: fmin fmax points");
                return result;
            }
            double fmin = 0.0;
            double fmax = 0.0;
            if (!parseDoubleToken(args.at(i + 1), fmin) || !parseDoubleToken(args.at(i + 2), fmax)) {
                result.errorMessage = QStringLiteral("Invalid numeric values for -f/--freq option");
                return result;
            }
            bool okPoints = false;
            int points = args.at(i + 3).toInt(&okPoints);
            if (!okPoints || points <= 0) {
                result.errorMessage = QStringLiteral("Frequency point count for -f/--freq must be a positive integer");
                return result;
            }
            if (fmax <= fmin) {
                result.errorMessage = QStringLiteral("Frequency maximum must be greater than minimum for -f/--freq");
                return result;
            }
            options.freqSpecified = true;
            options.fmin = fmin;
            options.fmax = fmax;
            options.freqPoints = points;
            i += 4;
            continue;
        }

        if (!treatAsPositional && (arg == QStringLiteral("-c") || arg == QStringLiteral("--cascade"))) {
            ++i;
            if (i >= args.size()) {
                result.errorMessage = QStringLiteral("Option -c/--cascade requires at least one network specification");
                return result;
            }
            if (!parseCascadeItems(args, i, options, result.errorMessage))
                return result;
            continue;
        }

        options.files.append(arg);
        ++i;
    }

    result.ok = true;
    return result;
}

QString CommandLineParser::helpText() const
{
    return QStringLiteral(
        "Usage: fsnpview [files...] [options]\n"
        "\n"
        "Positional arguments:\n"
        "  files...                 One or more Touchstone files (.sNp) to open.\n"
        "\n"
        "Options:\n"
        "  -c, --cascade <items>    Cascade file or lumped networks in order. Each item\n"
        "                           is either a file path or a lumped element name with\n"
        "                           optional parameter/value pairs.\n"
        "  -f, --freq <fmin> <fmax> <points>\n"
        "                           Set frequency range in Hz and number of points.\n"
        "  -s, --save <file>        Save cascaded result to the specified .s2p file.\n"
        "  -n, --nogui              Run without launching the GUI.\n"
        "  -h, --help               Show this help message.\n"
        "\n"
        "Available lumped networks (case insensitive):\n"
        "  R_series          R (Ohm)        default 50\n"
        "  R_shunt           R (Ohm)        default 50\n"
        "  C_series          C (pF)         default 1\n"
        "  C_shunt           C (pF)         default 1\n"
        "  L_series          L (nH), R_ser (Ohm)    defaults 1, 1\n"
        "  L_shunt           L (nH), R_ser (Ohm)    defaults 1, 1\n"
        "  TransmissionLine  len (mm), Z0 (Ohm), er_eff   defaults 1, 50, 1\n"
        "  TL_lossy          len (mm), Z0 (Ohm), er_eff, a (dB/m), a_d (dB/m), fa (Hz)\n"
        "                    defaults 1, 50, 1, 10, 1, 1e9\n"
        "  LRC_ser_shunt     L (nH), R (Ohm), C (pF) defaults 1, 1e-3, 1\n"
        "  LRC_par_ser       L (nH), R (Ohm), C (pF) defaults 1, 1e6, 1\n"
        "\n"
        "Examples:\n"
        "  fsnpview example.s2p -c example.s2p R_series R 75\n"
        "  fsnpview -n -c input.s2p TL len 2 Z0 75 er_eff 2.9 -f 1e6 1e9 1001 -s result.s2p\n");
}

