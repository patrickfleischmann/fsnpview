#pragma once

#include <QString>
#include <QStringList>
#include <QVector>
#include <optional>

#include "networklumped.h"

class CommandLineParser
{
public:
    struct ParameterOverride
    {
        int index = -1;
        double value = 0.0;
    };

    struct CascadeEntry
    {
        enum class Type
        {
            File,
            Lumped
        };

        Type type = Type::File;
        QString identifier; // path or descriptive name
        NetworkLumped::NetworkType lumpedType = NetworkLumped::NetworkType::R_series;
        QVector<ParameterOverride> parameterOverrides;
    };

    struct Options
    {
        QStringList files;
        QVector<CascadeEntry> cascade;
        bool cascadeRequested = false;
        bool helpRequested = false;
        bool noGui = false;
        bool freqSpecified = false;
        double fmin = 0.0;
        double fmax = 0.0;
        int freqPoints = 0;
        bool saveRequested = false;
        QString savePath;
        bool argumentsProvided = false;
    };

    struct ParseResult
    {
        Options options;
        bool ok = false;
        QString errorMessage;
    };

    ParseResult parse(int argc, char *argv[]) const;
    QString helpText() const;
};

